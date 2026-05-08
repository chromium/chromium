// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/file_system_id.h"

#include "base/files/file.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#endif

namespace sqlite_vfs {

std::optional<FileSystemId> GetFileSystemId(Client client,
                                            const base::File& file) {
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  if (file.IsValid()) {
#if BUILDFLAG(IS_WIN)
    BY_HANDLE_FILE_INFORMATION info;
    if (::GetFileInformationByHandle(file.GetPlatformFile(), &info)) {
      return FileSystemId{
          .volume_serial_number = info.dwVolumeSerialNumber,
          .file_index_high = info.nFileIndexHigh,
          .file_index_low = info.nFileIndexLow,
      };
    }
#elif BUILDFLAG(IS_POSIX)
    base::stat_wrapper_t stat_info;
    if (base::File::Fstat(file.GetPlatformFile(), &stat_info) == 0) {
      return FileSystemId{
          .dev = static_cast<dev_t>(stat_info.st_dev),
          .ino = static_cast<ino_t>(stat_info.st_ino),
      };
    }
#endif
    error = base::File::GetLastFileError();
  }

  base::UmaHistogramExactLinear(
      GetHistogramName(client, "GetFileSystemIdError"), -error,
      -base::File::FILE_ERROR_MAX);
  return std::nullopt;
}

}  // namespace sqlite_vfs
