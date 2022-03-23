// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/util/mrfu_cache.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {
namespace internal {

using Results = std::vector<base::FilePath>;
using ScoredResults = std::vector<std::pair<base::FilePath, float>>;
using ValidAndInvalidResults = std::pair<ScoredResults, Results>;

}  // namespace internal

// ZeroStateFileProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class ZeroStateFileProvider : public SearchProvider,
                              file_manager::file_tasks::FileTasksObserver {
 public:
  explicit ZeroStateFileProvider(Profile* profile);

  ZeroStateFileProvider(const ZeroStateFileProvider&) = delete;
  ZeroStateFileProvider& operator=(const ZeroStateFileProvider&) = delete;

  ~ZeroStateFileProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;
  bool ShouldBlockZeroState() const override;

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;

 private:
  // Takes a pair of vectors: <valid paths, invalid paths>, and converts the
  // valid paths to ZeroStatFilesResults and sets them as this provider's
  // results. The invalid paths are removed from the model.
  void SetSearchResults(internal::ValidAndInvalidResults results);

  // TODO(crbug.com/1216084): Remove this after finishing developing Continue
  // Section. Appends mock results to the driver provider.
  void AppendFakeSearchResults(Results* results);

  void OnProtoInitialized(ReadStatus status);

  // The reference to profile to get ZeroStateFileProvider service.
  Profile* const profile_;

  ash::ThumbnailLoader thumbnail_loader_;

  // The ranking model used to produce local file results for searches with an
  // empty query.
  std::unique_ptr<MrfuCache> files_ranker_;

  base::TimeTicks query_start_time_;

  // A file needs to have been modified more recently than this to be considered
  // valid.
  const base::TimeDelta max_last_modified_time_;

  base::ScopedObservation<file_manager::file_tasks::FileTasksNotifier,
                          file_manager::file_tasks::FileTasksObserver>
      file_tasks_observer_{this};

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ZeroStateFileProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_
