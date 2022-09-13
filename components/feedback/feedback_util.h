// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/feedback/feedback_common.h"

namespace feedback_util {

bool ZipString(const base::FilePath& filename,
               const std::string& data,
               std::string* compressed_data);

// Converts the entries in |sys_info| into a single string. Primarily used for
// creating a system_logs.txt file attached to feedback reports.
std::string LogsToString(const FeedbackCommon::SystemLogsMap& sys_info);

#if !BUILDFLAG(IS_WIN)
// Returns true if the data from the file specified by |path| is read into
// |contents| successfully.
// If the file size is greater than |max_size| in bytes, the data will be
// truncated to |max_size| and put in |contents|.
bool ReadEndOfFile(const base::FilePath& path,
                   size_t max_size,
                   std::string* contents);
#endif

}  // namespace feedback_util

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
