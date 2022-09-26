// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_SUGGESTION_PROVIDER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/files/file_suggestion_provider.h"
#include "chrome/browser/ui/app_list/search/util/mrfu_cache.h"

class Profile;

namespace app_list {

// This is the provider for local file suggestions.
// Currently stubbed.
class LocalFileSuggestionProvider
    : public FileSuggestionProvider,
      public file_manager::file_tasks::FileTasksObserver {
 public:
  LocalFileSuggestionProvider(
      Profile* profile,
      base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback);
  LocalFileSuggestionProvider(const LocalFileSuggestionProvider&) = delete;
  LocalFileSuggestionProvider& operator=(const LocalFileSuggestionProvider&) =
      delete;
  ~LocalFileSuggestionProvider() override;

  // FileSuggestionProvider:
  void GetSuggestFileData(GetSuggestFileDataCallback callback) override;

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_SUGGESTION_PROVIDER_H_
