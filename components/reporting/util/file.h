// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for file operations.

#ifndef COMPONENTS_REPORTING_UTIL_FILE_H_
#define COMPONENTS_REPORTING_UTIL_FILE_H_

#include "base/files/file_util.h"

namespace reporting {

// Deletes the given path, whether it's a file or a directory.
// This function is identical to base::DeleteFile() except that it issues a
// warning if the deletion fails. Useful when we do not care about whether the
// deletion succeeds or not.
bool DeleteFileWarnIfFailed(const base::FilePath& path);

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_FILE_H_
