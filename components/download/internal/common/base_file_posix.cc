// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <errno.h>

#include "base/files/file_util.h"
#include "components/download/public/common/download_file_utils.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"

namespace download {

DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  if (!ReplaceFileWithPermissions(full_path_, new_path)) {
    base::File::Error file_error = base::File::OSErrorToFileError(errno);
    return ConvertFileErrorToInterruptReason(file_error);
  }
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace download
