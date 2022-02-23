// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for file operations.

#ifndef COMPONENTS_REPORTING_UTIL_FILE_H_
#define COMPONENTS_REPORTING_UTIL_FILE_H_

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"

namespace reporting {

// Deletes the given path, whether it's a file or a directory.
// This function is identical to base::DeleteFile() except that it issues a
// warning if the deletion fails. Useful when we do not care about whether the
// deletion succeeds or not.
bool DeleteFileWarnIfFailed(const base::FilePath& path);

// Enumerates over |dir_enum|, and deletes the file if pred.Run(file) returns
// true. If |pred| is unspecified, all files enumerated are deleted. It deletes
// each individual file with DeleteFileWarnIfFailed(). Refer to
// DeleteFileWarnIfFailed() for the effect of the deletion.
// Returns true if all files are deleted successfully, otherwise returns false.
bool DeleteFilesWarnIfFailed(
    base::FileEnumerator& dir_enum,
    base::RepeatingCallback<bool(const base::FilePath&)> pred =
        base::BindRepeating([](const base::FilePath&) { return true; }));

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_FILE_H_
