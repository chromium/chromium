// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"

namespace app_list {
namespace {

// The prefix of a drive file suggestion id.
constexpr char kDriveFileSuggestionPrefix[] = "zero_state_drive://";

// The prefix of a local file suggestion id.
constexpr char kLocalFileSuggestionPrefix[] = "zero_state_file://";

// Returns the prefix that matches `type`.
std::string GetPrefixFromSuggestionType(FileSuggestionType type) {
  switch (type) {
    case FileSuggestionType::kDriveFile:
      return kDriveFileSuggestionPrefix;
    case FileSuggestionType::kLocalFile:
      return kLocalFileSuggestionPrefix;
  }
}

}  // namespace

// FileSuggestData -------------------------------------------------------

FileSuggestData::FileSuggestData(
    FileSuggestionType new_type,
    const base::FilePath& new_file_path,
    const absl::optional<std::string>& new_prediction_reason)
    : type(new_type),
      file_path(new_file_path),
      id(GetPrefixFromSuggestionType(type) + file_path.value()),
      prediction_reason(new_prediction_reason) {}

FileSuggestData::FileSuggestData(FileSuggestData&&) = default;

FileSuggestData::FileSuggestData(const FileSuggestData&) = default;

FileSuggestData& FileSuggestData::operator=(const FileSuggestData&) = default;

FileSuggestData::~FileSuggestData() = default;

}  // namespace app_list
