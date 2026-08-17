// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/file_system_id.h"

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <algorithm>
#include <array>
#elif BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#endif

namespace sqlite_vfs {

std::optional<FileSystemId> GetFileSystemId(Client client,
                                            const base::File& file) {
  std::optional<base::File::Error> error;

  if (file.IsValid()) {
#if BUILDFLAG(IS_WIN)
    // Try to get the full 128-bit identifier for the file. Note that not all
    // versions of Windows and all filesystems support this.
    FILE_ID_INFO id_info = {};
    if (::GetFileInformationByHandleEx(file.GetPlatformFile(), FileIdInfo,
                                       &id_info, sizeof(id_info)) &&
        !std::ranges::all_of(id_info.FileId.Identifier,
                             [](uint8_t b) { return b == 0; })) {
      return FileSystemId{
          .volume_serial_number = id_info.VolumeSerialNumber,
          .file_id = std::to_array(id_info.FileId.Identifier),
      };
    }

    // Fall-back to getting the 64-bit file index.
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(file.GetPlatformFile(), &info)) {
      error = base::File::GetLastFileError();
    } else if (info.nFileIndexHigh != 0 || info.nFileIndexLow != 0) {
      FileSystemId id{
          .volume_serial_number = info.dwVolumeSerialNumber,
      };
      const uint64_t file_index =
          (static_cast<uint64_t>(info.nFileIndexHigh) << 32) |
          info.nFileIndexLow;
      base::span(id.file_id)
          .copy_prefix_from(base::byte_span_from_ref(file_index));
      return id;
    }
#elif BUILDFLAG(IS_POSIX)
    base::stat_wrapper_t stat_info;
    if (base::File::Fstat(file.GetPlatformFile(), &stat_info) != 0) {
      error = base::File::GetLastFileError();
    } else if (stat_info.st_ino != 0) {
      return FileSystemId{
          .dev = static_cast<dev_t>(stat_info.st_dev),
          .ino = static_cast<ino_t>(stat_info.st_ino),
      };
    }
#endif
  } else {
    error = base::File::FILE_ERROR_FAILED;
  }

  // Record `FILE_OK` if the OS returned a degenerate identifier.
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "GetFileSystemIdError"),
      -error.value_or(base::File::FILE_OK), -base::File::FILE_ERROR_MAX);
  return std::nullopt;
}

}  // namespace sqlite_vfs
