// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/file.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace reporting {

bool DeleteFileWarnIfFailed(const base::FilePath& path) {
  const auto delete_result = base::DeleteFile(path);
  if (!delete_result) {
    LOG(WARNING) << "Failed to delete " << path.MaybeAsASCII();
  }
  return delete_result;
}

}  // namespace reporting
