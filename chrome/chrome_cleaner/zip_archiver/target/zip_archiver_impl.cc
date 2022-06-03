// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/target/zip_archiver_impl.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "third_party/zlib/contrib/minizip/ioapi.h"
#include "third_party/zlib/contrib/minizip/zip.h"
#include "third_party/zlib/zlib.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

constexpr int64_t kReadBufferSize = 4096;

// Compression method is STORE(0)
constexpr int16_t kCompressionMethod = 0;

// Section 4.4.2 http://www.pkware.com/documents/casestudies/APPNOTE.TXT
// The lower byte indicates the ZIP version(63 = 6.3) and the upper byte
// indicates the file attribute compatibility(0 = MS-DOS). We would like to
// choose the lowest version as possible to make it easier to decompress. ZIP
// 6.3 is the lowest version which supports UTF-8 filename.
constexpr uint16_t kVersionMadeBy = (0x0 << 8) | 63;

// Section 4.4.4 http://www.pkware.com/documents/casestudies/APPNOTE.TXT
// Setting the Language encoding flag so the file is told to be in UTF-8.
constexpr uint16_t kLanguageEncodingFlag = 0x1 << 11;

bool CalculateFileCrc32(base::File* file,
                        const int64_t length,
                        uint32_t* crc32_result) {
  DCHECK(file);
  DCHECK(crc32_result);

  std::vector<char> buffer(kReadBufferSize);
  int64_t offset = 0;
  uint32_t crc32_value = 0;

  while (offset < length) {
    static_assert(kReadBufferSize < std::numeric_limits<int>::max(),
                  "kReadBufferSize too large.");
    const int read_size = file->Read(
        offset, buffer.data(),
        static_cast<int>(std::min(length - offset, kReadBufferSize)));
    if (read_size <= 0) {
      LOG(ERROR) << "Unable to read the file when calculating CRC32.";
      return false;
    }
    CHECK_LE(base::checked_cast<size_t>(read_size), buffer.size());

    crc32_value = crc32(crc32_value, reinterpret_cast<uint8_t*>(buffer.data()),
                        read_size);

    offset += read_size;
  }

  *crc32_result = crc32_value;
  return true;
}

// Set up the minizip IO interface. The default functions of the interface are
// POSIX IO functions, which work with |FILE*| file objects. Therefore, we hook
// the open function of the interface and directly return the |FILE*| object of
// the opened zip file. So other default functions can operate correctly with
// the returned |FILE*| object, including closing the file.
bool InitializeZipIOInterface(base::File output_file,
                              zlib_filefunc64_def* zip_func_table) {
  DCHECK(zip_func_table);

  FILE* output_file_ptr = base::FileToFILE(std::move(output_file), "wb");
  if (output_file_ptr == nullptr) {
    LOG(ERROR) << "Unable to open FILE* from the base::File object.";
    return false;
  }

  // Initialize with the default POSIX IO functions.
  fill_fopen64_filefunc(zip_func_table);
  // Now the |output_file_ptr| is owned by the minizip.
  zip_func_table->opaque = output_file_ptr;
  // Return the |FILE*| object of the opened zip file.
  zip_func_table->zopen64_file = [](void* opaque, const void* /*filename*/,
                                    int /*mode*/) { return opaque; };

  return true;
}

zip_fileinfo TimeToZipFileInfo(const base::Time& file_time) {
  base::Time::Exploded file_time_parts;
  file_time.LocalExplode(&file_time_parts);

  zip_fileinfo zip_info = {};
  // This if check works around the handling of the year value in
  // contrib/minizip/zip.c in function zip64local_TmzDateToDosDate
  // It assumes that dates below 1980 are in the double digit format.
  // Hence the fail safe option is to leave the date unset. Some programs
  // might show the unset date as 1980-0-0 which is invalid.
  if (file_time_parts.year >= 1980) {
    zip_info.tmz_date.tm_year = file_time_parts.year;
    zip_info.tmz_date.tm_mon = file_time_parts.month - 1;
    zip_info.tmz_date.tm_mday = file_time_parts.day_of_month;
    zip_info.tmz_date.tm_hour = file_time_parts.hour;
    zip_info.tmz_date.tm_min = file_time_parts.minute;
    zip_info.tmz_date.tm_sec = file_time_parts.second;
  }

  return zip_info;
}

ZipArchiverResultCode AddToArchive(zipFile zip_object,
                                   const std::string& filename_in_zip,
                                   const std::string& password,
                                   base::File src_file) {
  base::File::Info file_info;
  if (!src_file.GetInfo(&file_info)) {
    LOG(ERROR) << "Unable to get the file information.";
    return ZipArchiverResultCode::kErrorIO;
  }

  if (file_info.is_directory || file_info.is_symbolic_link) {
    LOG(ERROR) << "The source file is a directory or a symbolic link.";
    return ZipArchiverResultCode::kErrorInvalidParameter;
  }

  const int64_t src_length = file_info.size;

  if (src_length > kQuarantineSourceSizeLimit) {
    LOG(ERROR) << "Source file is too big.";
    return ZipArchiverResultCode::kErrorSourceFileTooBig;
  }

  uint32_t src_crc32 = 0;
  if (!CalculateFileCrc32(&src_file, src_length, &src_crc32)) {
    LOG(ERROR) << "Failed to calculate the CRC32 of the source file.";
    return ZipArchiverResultCode::kErrorIO;
  }

  const zip_fileinfo zip_info = TimeToZipFileInfo(file_info.last_modified);

  if (zipOpenNewFileInZip4_64(
          zip_object, filename_in_zip.c_str(), &zip_info,
          /*extrafield_local=*/nullptr, /*size_extrafield_local=*/0,
          /*extrafield_global=*/nullptr, /*size_extrafield_global=*/0,
          /*comment=*/nullptr, kCompressionMethod, /*level=*/0, /*raw=*/0,
          -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, password.c_str(),
          src_crc32, kVersionMadeBy, kLanguageEncodingFlag,
          /*zip64=*/1) != Z_OK) {
    LOG(ERROR) << "Unable to create a file entry in the zip.";
    return ZipArchiverResultCode::kErrorMinizipInternal;
  }

  std::vector<char> buffer(kReadBufferSize);
  int64_t src_offset = 0;
  while (src_offset < src_length) {
    static_assert(kReadBufferSize <= std::numeric_limits<int>::max(),
                  "kReadBufferSize too large.");
    const int read_size = src_file.Read(
        src_offset, buffer.data(),
        static_cast<int>(std::min(src_length - src_offset, kReadBufferSize)));
    if (read_size <= 0) {
      LOG(ERROR) << "Unable to read the source file when archiving.";
      return ZipArchiverResultCode::kErrorIO;
    }
    CHECK_LE(base::checked_cast<size_t>(read_size), buffer.size());
    if (zipWriteInFileInZip(zip_object, buffer.data(), read_size) != Z_OK) {
      LOG(ERROR) << "Unable to write data into the zip.";
      return ZipArchiverResultCode::kErrorMinizipInternal;
    }
    src_offset += read_size;
  }

  if (zipCloseFileInZip(zip_object) != Z_OK) {
    LOG(ERROR) << "Unable to close the file entry.";
    return ZipArchiverResultCode::kErrorMinizipInternal;
  }

  return ZipArchiverResultCode::kSuccess;
}

}  // namespace

ZipArchiverImpl::ZipArchiverImpl(
    mojo::PendingReceiver<mojom::ZipArchiver> receiver,
    base::OnceClosure connection_error_handler)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(connection_error_handler));
}

ZipArchiverImpl::~ZipArchiverImpl() = default;

void ZipArchiverImpl::Archive(mojo::PlatformHandle src_file_handle,
                              mojo::PlatformHandle zip_file_handle,
                              const std::string& filename_in_zip,
                              const std::string& password,
                              ArchiveCallback callback) {
  // Neither |filename_in_zip| nor |password| being empty will raise any error
  // in the minizip internally. However, the produced zip file can't be
  // decompressed by some zip tools. So these cases are rejected.
  if (filename_in_zip.empty() || password.empty()) {
    LOG(ERROR) << "Either filename or password is empty.";
    std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
    return;
  }

  base::File src_file(src_file_handle.TakeHandle());
  if (!src_file.IsValid()) {
    LOG(ERROR) << "Source file is invalid.";
    std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
    return;
  }

  base::File zip_file(zip_file_handle.TakeHandle());
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Destination file is invalid.";
    std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
    return;
  }

  zlib_filefunc64_def zip_func_table;
  if (!InitializeZipIOInterface(std::move(zip_file), &zip_func_table)) {
    LOG(ERROR) << "Failed to initialize the minizip IO interface.";
    std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
    return;
  }

  // Since the open function has been hooked in |InitializeZipIOInterface|,
  // which doesn't need a file path, we just pass a dummy path to the |path|.
  zipFile zip_object = zipOpen2_64(/*path=*/"", /*append=*/0,
                                   /*globalcomment=*/nullptr, &zip_func_table);
  if (zip_object == nullptr) {
    LOG(ERROR) << "Unable to open the zip file.";
    std::move(callback).Run(ZipArchiverResultCode::kErrorMinizipInternal);
    return;
  }

  ZipArchiverResultCode result_code =
      AddToArchive(zip_object, filename_in_zip, password, std::move(src_file));

  if (zipClose(zip_object, /*global_comment=*/nullptr) != Z_OK) {
    LOG(ERROR) << "Unable to close the zip file.";
    if (result_code == ZipArchiverResultCode::kSuccess)
      result_code = ZipArchiverResultCode::kErrorMinizipInternal;
  }

  std::move(callback).Run(result_code);
}

}  // namespace chrome_cleaner
