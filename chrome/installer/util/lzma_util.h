// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LZMA_UTIL_H_
#define CHROME_INSTALLER_UTIL_LZMA_UTIL_H_

#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/win/windows_types.h"

// The error status of LzmaUtil::Unpack which is used to publish metrics. Do not
// change the order.
enum UnPackStatus {
  UNPACK_NO_ERROR = 0,
  UNPACK_ARCHIVE_NOT_FOUND = 1,
  UNPACK_ARCHIVE_CANNOT_OPEN = 2,
  UNPACK_SZAREX_OPEN_ERROR = 3,
  UNPACK_EXTRACT_ERROR = 4,
  UNPACK_EXTRACT_EXCEPTION = 5,
  UNPACK_NO_FILENAME_ERROR = 6,
  UNPACK_CREATE_FILE_ERROR = 7,
  UNPACK_WRITE_FILE_ERROR = 8,
  // UNPACK_SET_FILE_TIME_ERROR = 9, Deprecated.
  // UNPACK_CLOSE_FILE_ERROR = 10, Deprecated.
  UNPACK_ALLOCATE_ERROR = 11,
  UNPACK_CRC_ERROR = 12,
  UNPACK_DISK_FULL = 13,
  UNPACK_IO_DEVICE_ERROR = 14,
  UNPACK_STATUS_COUNT,
};

// Unpacks the contents of |archive| into |output_dir|. |output_file|, if not
// null, is populated with the name of the last (or only) member extracted from
// the archive. Returns UNPACK_NO_ERROR on success. Otherwise, returns a status
// value indicating the operation that failed.
UnPackStatus UnPackArchive(const base::FilePath& archive,
                           const base::FilePath& output_dir,
                           base::FilePath* output_file);

// A utility class that wraps LZMA SDK library. Prefer UnPackArchive over using
// this class directly.
class LzmaUtilImpl {
 public:
  LzmaUtilImpl();

  LzmaUtilImpl(const LzmaUtilImpl&) = delete;
  LzmaUtilImpl& operator=(const LzmaUtilImpl&) = delete;

  ~LzmaUtilImpl();

  UnPackStatus OpenArchive(const base::FilePath& archivePath);

  // Unpacks the archive to the given location
  UnPackStatus UnPack(const base::FilePath& location);

  // Unpacks the archive to the given location and returns the last file
  // extracted from archive.
  UnPackStatus UnPack(const base::FilePath& location,
                      base::FilePath* output_file);

  std::optional<DWORD> GetErrorCode() { return error_code_; }

  void CloseArchive();

 protected:
  bool CreateDirectory(const base::FilePath& dir);

 private:
  base::File archive_file_;
  std::optional<DWORD> error_code_;
};

#endif  // CHROME_INSTALLER_UTIL_LZMA_UTIL_H_
