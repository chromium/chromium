// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/tutorial_factory_helper.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/internal/tutorial_fetcher.h"
#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"
#include "chrome/browser/video_tutorials/internal/tutorial_service_impl.h"
#include "chrome/browser/video_tutorials/internal/tutorial_store.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace video_tutorials {
namespace {
const base::FilePath::CharType kVideoTutorialsDbName[] =
    FILE_PATH_LITERAL("VideoTutorialsDatabase");
}  // namespace

std::unique_ptr<VideoTutorialService> CreateVideoTutorialService(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    const std::string& accepted_language,
    const std::string& country_code,
    const std::string& api_key,
    const std::string& client_version,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& default_server_url,
    PrefService* pref_service) {
  // Create tutorial store and manager.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::FilePath database_dir = storage_dir.Append(kVideoTutorialsDbName);
  auto tutorial_db =
      db_provider->GetDB<video_tutorials::proto::VideoTutorialGroups>(
          leveldb_proto::ProtoDbType::VIDEO_TUTORIALS_V2_DATABASE, database_dir,
          task_runner);
  auto tutorial_store = std::make_unique<TutorialStore>(std::move(tutorial_db));
  auto tutorial_manager = std::make_unique<TutorialManagerImpl>(
      std::move(tutorial_store), pref_service);

  // Create fetcher.
  auto fetcher = TutorialFetcher::Create(
      Config::GetTutorialsServerURL(default_server_url), country_code,
      accepted_language, api_key, Config::GetExperimentTag(), client_version,
      url_loader_factory);

  auto tutorial_service_impl = std::make_unique<TutorialServiceImpl>(
      std::move(tutorial_manager), std::move(fetcher), pref_service);
  return std::move(tutorial_service_impl);
}

}  // namespace video_tutorials
