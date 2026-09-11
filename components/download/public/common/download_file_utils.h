// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_UTILS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "components/download/public/common/download_export.h"

namespace download {

// Atomically replaces `to_path` with `from_path`, attempting to preserve
// `to_path`'s permissions (or default permissions if `to_path` does not exist).
COMPONENTS_DOWNLOAD_EXPORT bool ReplaceFileWithPermissions(
    const base::FilePath& from_path,
    const base::FilePath& to_path);

// Atomically writes `data` to `filename` by first writing to a temporary file
// in the same directory and replacing `filename` while preserving existing
// permissions.
COMPONENTS_DOWNLOAD_EXPORT bool WriteFileAtomicallyWithPermissions(
    const base::FilePath& path,
    base::span<const uint8_t> data);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_UTILS_H_
