// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/local_file_suggestion_provider.h"
#include "base/notreached.h"

namespace app_list {

LocalFileSuggestionProvider::LocalFileSuggestionProvider(
    Profile* profile,
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : FileSuggestionProvider(notify_update_callback) {}

LocalFileSuggestionProvider::~LocalFileSuggestionProvider() = default;

void LocalFileSuggestionProvider::GetSuggestFileData(
    GetSuggestFileDataCallback callback) {
  // TODO(): Add the callback to a CallbackList to be called when validation is
  // complete, and fire off validation if it's not already in progress. This
  // approach makes sure we won't have issues if multiple clients request data
  // in rapid succession.
  std::move(callback).Run(absl::nullopt);
}

void LocalFileSuggestionProvider::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  // TODO: Add all applicable files to `files_ranker_`, then call
  // `NotifySuggestionsUpdate`, limited by a debounce timer to prevent
  // issues with multiple files being opened at once.
  NOTIMPLEMENTED();
}

}  // namespace app_list
