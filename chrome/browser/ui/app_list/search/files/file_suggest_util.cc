// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"

namespace app_list {

// FileSuggestData -------------------------------------------------------

FileSuggestData::FileSuggestData() = default;

FileSuggestData::FileSuggestData(
    const base::FilePath& new_file_path,
    const absl::optional<std::string>& new_prediction_reason)
    : file_path(new_file_path), prediction_reason(new_prediction_reason) {}

FileSuggestData::FileSuggestData(FileSuggestData&&) = default;

FileSuggestData::FileSuggestData(const FileSuggestData&) = default;

FileSuggestData& FileSuggestData::operator=(const FileSuggestData&) = default;

FileSuggestData::~FileSuggestData() = default;

}  // namespace app_list
