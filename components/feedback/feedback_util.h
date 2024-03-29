// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/feedback/feedback_common.h"

namespace feedback_util {

// Creates a ZIP file that contains a single file within. That file is named
// `filename` and its contents is `data`. Returns the compressed archive, or
// nullopt on failure.
std::optional<std::string> ZipString(const base::FilePath& filename,
                                     std::string_view data);

// Converts the entries in `sys_info` into a single string. Primarily used for
// creating a system_logs.txt file attached to feedback reports.
std::string LogsToString(const FeedbackCommon::SystemLogsMap& sys_info);

// Removes URL fields from the autofill logs.
void RemoveUrlsFromAutofillData(std::string& autofill_metadata);

#if !BUILDFLAG(IS_WIN)
// Reads the data from the latter part of the file specified by `path`.
// If the file size is greater than `max_size` in bytes, the data will be
// truncated to `max_size`. Returns the file contents, or nullopt on failure.
std::optional<std::string> ReadEndOfFile(const base::FilePath& path,
                                         size_t max_size);
#endif

}  // namespace feedback_util

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
