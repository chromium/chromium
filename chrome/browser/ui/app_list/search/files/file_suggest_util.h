// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_

#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

// Outcome of a call to `FileSuggestKeyedService::GetSuggestFileData()`. These
// values persist to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class DriveSuggestValidationStatus {
  kOk = 0,
  kDriveFSNotMounted = 1,
  kNoResults = 2,
  kPathLocationFailed = 3,
  kAllFilesErrored = 4,
  kDriveDisabled = 5,
  kMaxValue = kDriveDisabled,
};

// The data of an individual file suggested by `FileSuggestKeyedService`.
struct FileSuggestData {
  FileSuggestData();
  FileSuggestData(const base::FilePath& new_file_path,
                  const absl::optional<std::string>& new_prediction_reason);
  FileSuggestData(FileSuggestData&&);
  FileSuggestData(const FileSuggestData&);
  FileSuggestData& operator=(const FileSuggestData&);
  ~FileSuggestData();

  // The path to the suggested file.
  base::FilePath file_path;

  // The reason why the file is suggested.
  absl::optional<std::string> prediction_reason;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_
