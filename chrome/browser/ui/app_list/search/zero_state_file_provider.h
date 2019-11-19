// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_PROVIDER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {
namespace internal {

using Results = std::vector<base::FilePath>;
using ScoredResults = std::vector<std::pair<base::FilePath, float>>;
using ValidAndInvalidResults = std::pair<ScoredResults, Results>;

}  // namespace internal

class RecurrenceRanker;

// ZeroStateFileProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class ZeroStateFileProvider : public SearchProvider,
                              file_manager::file_tasks::FileTasksObserver {
 public:
  explicit ZeroStateFileProvider(Profile* profile);
  ~ZeroStateFileProvider() override;

  void Start(const base::string16& query) override;

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;

 private:
  // Takes a pair of vectors: <valid paths, invalid paths>, and converts the
  // valid paths to ZeroStatFilesResults and sets them as this provider's
  // results. The invalid paths are removed from the model.
  void SetSearchResults(const internal::ValidAndInvalidResults& results);

  // The reference to profile to get ZeroStateFileProvider service.
  Profile* const profile_;

  // The ranking model used to produce local file results for searches with an
  // empty query.
  std::unique_ptr<RecurrenceRanker> files_ranker_;

  base::TimeTicks query_start_time_;

  ScopedObserver<file_manager::file_tasks::FileTasksNotifier,
                 file_manager::file_tasks::FileTasksObserver>
      file_tasks_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ZeroStateFileProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ZeroStateFileProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_PROVIDER_H_
