// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include <ntstatus.h>
#include <windows.h>

#include <stddef.h>

#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/process/memory.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern "C" {
#include "third_party/lzma_sdk/7z.h"
#include "third_party/lzma_sdk/7zAlloc.h"
#include "third_party/lzma_sdk/7zCrc.h"
#include "third_party/lzma_sdk/7zFile.h"
}

namespace {

// define NTSTATUS to avoid including winternl.h
using NTSTATUS = LONG;

SRes LzmaReadFile(HANDLE file, void* data, size_t* size) {
  if (*size == 0)
    return SZ_OK;

  size_t processedSize = 0;
  DWORD maxSize = *size;
  do {
    DWORD processedLoc = 0;
    BOOL res = ReadFile(file, data, maxSize, &processedLoc, nullptr);
    data = (void*)((unsigned char*)data + processedLoc);
    maxSize -= processedLoc;
    processedSize += processedLoc;
    if (processedLoc == 0) {
      if (res)
        return SZ_ERROR_READ;
      else
        break;
    }
  } while (maxSize > 0);

  *size = processedSize;
  return SZ_OK;
}

SRes SzFileSeekImp(const ISeekInStream* object, Int64* pos, ESzSeek origin) {
  CFileInStream* s = CONTAINER_FROM_VTBL(object, CFileInStream, vt);
  LARGE_INTEGER value;
  value.LowPart = (DWORD)*pos;
  value.HighPart = (LONG)((UInt64)*pos >> 32);
  DWORD moveMethod;
  switch (origin) {
    case SZ_SEEK_SET:
      moveMethod = FILE_BEGIN;
      break;
    case SZ_SEEK_CUR:
      moveMethod = FILE_CURRENT;
      break;
    case SZ_SEEK_END:
      moveMethod = FILE_END;
      break;
    default:
      return SZ_ERROR_PARAM;
  }
  value.LowPart = SetFilePointer(s->file.handle, value.LowPart, &value.HighPart,
                                 moveMethod);
  *pos = ((Int64)value.HighPart << 32) | value.LowPart;
  return ((value.LowPart == 0xFFFFFFFF) && (GetLastError() != ERROR_SUCCESS))
             ? SZ_ERROR_FAIL
             : SZ_OK;
}

SRes SzFileReadImp(const ISeekInStream* object, void* buffer, size_t* size) {
  CFileInStream* s = CONTAINER_FROM_VTBL(object, CFileInStream, vt);
  return LzmaReadFile(s->file.handle, buffer, size);
}

// Returns EXCEPTION_EXECUTE_HANDLER and populates |status| with the underlying
// NTSTATUS code for paging errors encountered while accessing file-backed
// mapped memory. Otherwise, return EXCEPTION_CONTINUE_SEARCH.
DWORD FilterPageError(const base::MemoryMappedFile& mapped_file,
                      DWORD exception_code,
                      const EXCEPTION_POINTERS* info,
                      int32_t* status) {
  if (exception_code != EXCEPTION_IN_PAGE_ERROR)
    return EXCEPTION_CONTINUE_SEARCH;

  const EXCEPTION_RECORD* exception_record = info->ExceptionRecord;
  const uint8_t* address = reinterpret_cast<const uint8_t*>(
      exception_record->ExceptionInformation[1]);
  if (address < mapped_file.data() ||
      address >= mapped_file.data() + mapped_file.length()) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // Cast NTSTATUS to int32_t to avoid including winternl.h
  *status = exception_record->ExceptionInformation[2];

  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

UnPackStatus UnPackArchive(const base::FilePath& archive,
                           const base::FilePath& output_dir,
                           base::FilePath* output_file) {
  VLOG(1) << "Opening archive " << archive.value();
  LzmaUtilImpl lzma_util;
  UnPackStatus status;
  if ((status = lzma_util.OpenArchive(archive)) != UNPACK_NO_ERROR) {
    PLOG(ERROR) << "Unable to open install archive: " << archive.value();
  } else {
    VLOG(1) << "Uncompressing archive to path " << output_dir.value();
    if ((status = lzma_util.UnPack(output_dir, output_file)) != UNPACK_NO_ERROR)
      PLOG(ERROR) << "Unable to uncompress archive: " << archive.value();
  }

  if (status != UNPACK_NO_ERROR) {
    absl::optional<DWORD> error_code = lzma_util.GetErrorCode();
    if (error_code.value_or(ERROR_SUCCESS) == ERROR_DISK_FULL)
      return UNPACK_DISK_FULL;
    if (error_code.value_or(ERROR_SUCCESS) == ERROR_IO_DEVICE)
      return UNPACK_IO_DEVICE_ERROR;
  }

  return status;
}

LzmaUtilImpl::LzmaUtilImpl() = default;
LzmaUtilImpl::~LzmaUtilImpl() = default;

UnPackStatus LzmaUtilImpl::OpenArchive(const base::FilePath& archivePath) {
  // Make sure file is not already open.
  CloseArchive();

  archive_file_.Initialize(archivePath,
                           base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                               base::File::FLAG_WIN_SHARE_DELETE);
  if (archive_file_.IsValid())
    return UNPACK_NO_ERROR;
  error_code_ = ::GetLastError();
  return archive_file_.error_details() == base::File::FILE_ERROR_NOT_FOUND
             ? UNPACK_ARCHIVE_NOT_FOUND
             : UNPACK_ARCHIVE_CANNOT_OPEN;
}

UnPackStatus LzmaUtilImpl::UnPack(const base::FilePath& location) {
  return UnPack(location, nullptr);
}

UnPackStatus LzmaUtilImpl::UnPack(const base::FilePath& location,
                                  base::FilePath* output_file) {
  DCHECK(archive_file_.IsValid());

  CFileInStream archiveStream;
  archiveStream.file.handle = archive_file_.GetPlatformFile();
  archiveStream.vt.Read = SzFileReadImp;
  archiveStream.vt.Seek = SzFileSeekImp;

  CLookToRead2 lookStream;
  LookToRead2_CreateVTable(&lookStream, /*lookahead=*/False);
  const size_t kStreamBufferSize = 1 << 14;
  if (!base::UncheckedMalloc(kStreamBufferSize,
                             reinterpret_cast<void**>(&lookStream.buf))) {
    return UNPACK_ALLOCATE_ERROR;
  }
  std::unique_ptr<uint8_t, base::FreeDeleter> stream_buffer(lookStream.buf);
  lookStream.bufSize = kStreamBufferSize;
  LookToRead2_Init(&lookStream);
  lookStream.realStream = &archiveStream.vt;

  CrcGenerateTable();

  CSzArEx db;
  SzArEx_Init(&db);

  ISzAlloc allocImp = {SzAlloc, SzFree};
  ISzAlloc allocTempImp = {SzAllocTemp, SzFreeTemp};
  SRes sz_res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
  if (sz_res != SZ_OK) {
    LOG(ERROR) << "Error returned by SzArchiveOpen: " << sz_res;
    auto error_code = ::GetLastError();
    if (error_code != ERROR_SUCCESS)
      error_code_ = error_code;
    return UNPACK_SZAREX_OPEN_ERROR;
  }
  base::ScopedClosureRunner db_closer(
      base::BindOnce(&SzArEx_Free, &db, &allocImp));

  // Tracks the last folder that was uncompressed. The result is reused when
  // multiple subsequent files in the archive share the same folder.
  size_t last_folder_index = -1;
  // A mapping of either the target file (if the file exactly fits within a
  // folder) or a temporary file into which a folder is decompressed.
  absl::optional<base::MemoryMappedFile> mapped_file;
  for (size_t file_index = 0; file_index < db.NumFiles; ++file_index) {
    size_t file_name_length = SzArEx_GetFileNameUtf16(&db, file_index, nullptr);
    if (file_name_length < 1) {
      LOG(ERROR) << "Couldn't get file name";
      return UNPACK_NO_FILENAME_ERROR;
    }

    std::vector<UInt16> file_name(file_name_length);
    file_name_length =
        SzArEx_GetFileNameUtf16(&db, file_index, file_name.data());
    DCHECK_EQ(file_name_length, file_name.size());

    // |file_name| has a string terminator.
    base::FilePath file_path = location.Append(
        base::FilePath::StringType(file_name.begin(), --file_name.end()));

    if (output_file)
      *output_file = file_path;

    // If archive entry is directory create it and move on to the next entry.
    if (SzArEx_IsDir(&db, file_index)) {
      if (!CreateDirectory(file_path)) {
        error_code_ = ::GetLastError();
        return UNPACK_CREATE_FILE_ERROR;
      }
      continue;
    }

    CreateDirectory(file_path.DirName());

    base::File target_file(file_path, base::File::FLAG_CREATE_ALWAYS |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_WIN_EXCLUSIVE_READ |
                                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                          base::File::FLAG_CAN_DELETE_ON_CLOSE |
                                          base::File::FLAG_WIN_SHARE_DELETE);
    if (!target_file.IsValid()) {
      PLOG(ERROR) << "Invalid file.";
      error_code_ = ::GetLastError();
      return UNPACK_CREATE_FILE_ERROR;
    }
    // The target file is deleted by default unless extracting succeeds.
    target_file.DeleteOnClose(true);

    uint32_t folder_index = db.FileToFolder[file_index];

    // If |file_index| has no associated data to uncompress. The resulting file
    // is still written on disk and will be empty.
    if (folder_index != uint32_t(-1)) {
      uint64_t file_offset = db.UnpackPositions[file_index];

      uint64_t folder_offset =
          db.UnpackPositions[db.FolderToFile[folder_index]];
      CHECK_LE(folder_offset, file_offset);
      size_t file_offset_in_folder = (size_t)(file_offset - folder_offset);

      // |UnpackPositions| has NumFiles + 1 entries, with an extra entry
      // for the sentinel.
      size_t file_unpack_size =
          (size_t)(db.UnpackPositions[file_index + 1] - file_offset);
      uint64_t folder_unpack_size =
          SzAr_GetFolderUnpackSize(&db.db, folder_index);
      CHECK_LE(file_offset_in_folder + file_unpack_size, folder_unpack_size);

      // A buffer is used iff the folder doesn't match exactly the target file.
      // Otherwise, the target is written directly as a memory mapped file.
      // In practice, all folders are single file.
      bool use_temp_buffer = folder_unpack_size != file_unpack_size;
      if (last_folder_index != folder_index) {
        last_folder_index = folder_index;
        mapped_file.emplace();
        bool mapped_file_ok = false;
        if (use_temp_buffer) {
          base::FilePath temp_file_path;
          if (!base::CreateTemporaryFileInDir(location, &temp_file_path)) {
            error_code_ = ::GetLastError();
            return UNPACK_ALLOCATE_ERROR;
          }

          base::File temp_file(
              temp_file_path,
              base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_EXCLUSIVE_READ |
                  base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                  base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE |
                  base::File::FLAG_WIN_SHARE_DELETE);
          mapped_file_ok = mapped_file->Initialize(
              std::move(temp_file),
              {0, static_cast<size_t>(folder_unpack_size)},
              base::MemoryMappedFile::READ_WRITE_EXTEND);
        } else {
          mapped_file_ok = mapped_file->Initialize(
              target_file.Duplicate(),
              {0, static_cast<size_t>(folder_unpack_size)},
              base::MemoryMappedFile::READ_WRITE_EXTEND);
        }
        if (!mapped_file_ok) {
          PLOG(ERROR) << "Can't map file to memory.";
          error_code_ = ::GetLastError();
          return UNPACK_ALLOCATE_ERROR;
        }
        int32_t ntstatus = 0;  // STATUS_SUCCESS
        ::SetLastError(ERROR_SUCCESS);
        __try {
          sz_res = SzAr_DecodeFolder(&db.db, folder_index, &lookStream.vt,
                                     db.dataPos, mapped_file->data(),
                                     folder_unpack_size, &allocTempImp);
          if (sz_res != SZ_OK) {
            LOG(ERROR) << "Error returned by SzExtract: " << sz_res;
            auto error_code = ::GetLastError();
            if (error_code != ERROR_SUCCESS)
              error_code_ = error_code;
            return UNPACK_EXTRACT_ERROR;
          }
        } __except (FilterPageError(*mapped_file, GetExceptionCode(),
                                    GetExceptionInformation(), &ntstatus)) {
          LOG(ERROR)
              << "EXCEPTION_IN_PAGE_ERROR while accessing mapped memory; "
                 "NTSTATUS = "
              << ntstatus;
          // Return IO_DEVICE_ERROR for all known error except DISK_FULL,
          // IN_PAGE_ERROR and ACCESS_DENIED.
          switch (ntstatus) {
            case STATUS_DEVICE_DATA_ERROR:
            case STATUS_DEVICE_HARDWARE_ERROR:
            case STATUS_DEVICE_NOT_CONNECTED:
            case STATUS_INVALID_DEVICE_REQUEST:
            case STATUS_INVALID_LEVEL:
            case STATUS_IO_DEVICE_ERROR:
            case STATUS_IO_TIMEOUT:
            case STATUS_NO_SUCH_DEVICE:
              return UNPACK_IO_DEVICE_ERROR;
            case STATUS_DISK_FULL:
              return UNPACK_DISK_FULL;
            default:
              // This error indicates an unexpected error. Spikes in this are
              // worth investigation.
              return UNPACK_EXTRACT_EXCEPTION;
          }
        }
      }

      if (SzBitWithVals_Check(&db.CRCs, file_index)) {
        if (CrcCalc(mapped_file->data() + file_offset_in_folder,
                    file_unpack_size) != db.CRCs.Vals[file_index])
          return UNPACK_CRC_ERROR;
      }

      if (use_temp_buffer) {
        // Don't write all of the data at once because this can lead to kernel
        // address-space exhaustion on 32-bit Windows (see
        // https://crbug.com/1001022 for details).
        constexpr size_t kMaxWriteAmount = 8 * 1024 * 1024;
        for (size_t total_written = 0; total_written < file_unpack_size; /**/) {
          const size_t write_amount =
              std::min(kMaxWriteAmount, file_unpack_size - total_written);
          int written = target_file.WriteAtCurrentPos(
              reinterpret_cast<char*>(mapped_file->data() +
                                      file_offset_in_folder + total_written),
              write_amount);
          if (static_cast<size_t>(written) != write_amount) {
            PLOG(ERROR) << "Error returned by WriteFile";
            error_code_ = ::GetLastError();
            return UNPACK_WRITE_FILE_ERROR;
          }
          total_written += written;
        }
      } else {
        // Modified pages are not written to disk until they're evicted from the
        // working set. Explicitly kick off the write to disk now
        // (asynchronously) to improve the odds that the file's contents are
        // on-disk when another process (such as chrome.exe) would like to use
        // them.
        ::FlushViewOfFile(mapped_file->data(), 0);
        // Unmap the target file from the process's address space.
        mapped_file.reset();
        last_folder_index = -1;
        // Flush to avoid odd behavior, such as the bug in Windows 7 through
        // Windows 10 1809 for PE files described in
        // https://randomascii.wordpress.com/2018/02/25/compiler-bug-linker-bug-windows-kernel-bug/.
        // We've also observed oddly empty files on other Windows versions, so
        // this is unconditional.
        target_file.Flush();
      }
    }

    // On success, |target_file| is kept.
    target_file.DeleteOnClose(false);

    if (SzBitWithVals_Check(&db.MTime, file_index)) {
      if (!SetFileTime(target_file.GetPlatformFile(), nullptr, nullptr,
                       (const FILETIME*)(&db.MTime.Vals[file_index]))) {
        PLOG(ERROR) << "Error returned by SetFileTime";
        error_code_ = ::GetLastError();
        return UNPACK_SET_FILE_TIME_ERROR;
      }
    }
  }
  return UNPACK_NO_ERROR;
}

void LzmaUtilImpl::CloseArchive() {
  archive_file_.Close();
  error_code_ = absl::nullopt;
}

bool LzmaUtilImpl::CreateDirectory(const base::FilePath& dir) {
  bool result = true;
  if (directories_created_.find(dir) == directories_created_.end()) {
    result = base::CreateDirectory(dir);
    if (result)
      directories_created_.insert(dir);
  }
  return result;
}
