// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/video_tutorial_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/video_tutorials/tutorial_factory_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace video_tutorials {
namespace {

std::string GetCountryCode() {
  std::string country_code;
  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (!country_code.empty())
      return country_code;
    country_code = variations_service->GetLatestCountry();
  }
  return country_code;
}

std::string GetGoogleAPIKey() {
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  return is_stable_channel ? google_apis::GetAPIKey()
                           : google_apis::GetNonStableAPIKey();
}

}  // namespace

// static
VideoTutorialServiceFactory* VideoTutorialServiceFactory::GetInstance() {
  return base::Singleton<VideoTutorialServiceFactory>::get();
}

// static
VideoTutorialService* VideoTutorialServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<VideoTutorialService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

VideoTutorialServiceFactory::VideoTutorialServiceFactory()
    : SimpleKeyedServiceFactory("VideoTutorialService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(ImageFetcherServiceFactory::GetInstance());
  DependsOn(background_task::BackgroundTaskSchedulerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
VideoTutorialServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto* db_provider =
      ProfileKey::FromSimpleFactoryKey(key)->GetProtoDatabaseProvider();
  // |storage_dir| is not actually used since we are using the shared leveldb.
  base::FilePath storage_dir =
      ProfileKey::FromSimpleFactoryKey(key)->GetPath().Append(
          chrome::kVideoTutorialsStorageDirname);

  std::string accept_languanges =
      ProfileKey::FromSimpleFactoryKey(key)->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages);

  auto url_loader_factory =
      SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();

  base::Version version = version_info::GetVersion();
  std::string channel_name = chrome::GetChannelName();
  std::string client_version =
      base::StringPrintf("%d.%d.%d.%s.chrome",
                         version.components()[0],  // Major
                         version.components()[2],  // Build
                         version.components()[3],  // Patch
                         channel_name.c_str());

  return CreateVideoTutorialService(
      db_provider, storage_dir, accept_languanges, GetCountryCode(),
      GetGoogleAPIKey(), client_version, url_loader_factory,
      ProfileKey::FromSimpleFactoryKey(key)->GetPrefs());
}

}  // namespace video_tutorials
