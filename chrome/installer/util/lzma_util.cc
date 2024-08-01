// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/lzma_util.h"

#include <windows.h>

#include <ntstatus.h>
#include <stddef.h>

#include <set>

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "third_party/lzma_sdk/google/seven_zip_reader.h"

namespace {

class SevenZipDelegateImpl : public seven_zip::Delegate {
 public:
  SevenZipDelegateImpl(const base::FilePath& location,
                       base::FilePath* output_file);

  SevenZipDelegateImpl(const SevenZipDelegateImpl&) = delete;
  SevenZipDelegateImpl& operator=(const SevenZipDelegateImpl&) = delete;

  std::optional<DWORD> error_code() const { return error_code_; }
  UnPackStatus unpack_error() const { return unpack_error_; }

  // seven_zip::Delegate implementation:
  void OnOpenError(seven_zip::Result result) override;
  base::File OnTempFileRequest() override;
  bool OnEntry(const seven_zip::EntryInfo& entry,
               base::span<uint8_t>& temp_file) override;
  bool OnDirectory(const seven_zip::EntryInfo& entry) override;
  bool EntryDone(seven_zip::Result result,
                 const seven_zip::EntryInfo& entry) override;

 private:
  bool CreateDirectory(const base::FilePath& dir);

  const base::FilePath location_;
  const raw_ptr<base::FilePath> output_file_;

  std::set<base::FilePath> directories_created_;
  std::optional<DWORD> error_code_;
  base::File current_file_;
  std::optional<base::MemoryMappedFile> mapped_file_;
  UnPackStatus unpack_error_ = UNPACK_NO_ERROR;
};

SevenZipDelegateImpl::SevenZipDelegateImpl(const base::FilePath& location,
                                           base::FilePath* output_file)
    : location_(location), output_file_(output_file) {}

bool SevenZipDelegateImpl::CreateDirectory(const base::FilePath& dir) {
  bool result = true;
  if (directories_created_.find(dir) == directories_created_.end()) {
    result = base::CreateDirectory(dir);
    if (result)
      directories_created_.insert(dir);
  }
  return result;
}

void SevenZipDelegateImpl::OnOpenError(seven_zip::Result result) {
  switch (result) {
    case seven_zip::Result::kMalformedArchive:
    case seven_zip::Result::kBadCrc:
    case seven_zip::Result::kUnsupported: {
      auto error_code = ::GetLastError();
      if (error_code != ERROR_SUCCESS)
        error_code_ = error_code;
      unpack_error_ = UNPACK_SZAREX_OPEN_ERROR;
      break;
    }
    case seven_zip::Result::kFailedToAllocate:
      unpack_error_ = UNPACK_ALLOCATE_ERROR;
      break;
    case seven_zip::Result::kIoError:
      unpack_error_ = UNPACK_IO_DEVICE_ERROR;
      break;
    case seven_zip::Result::kUnknownError:
    case seven_zip::Result::kDiskFull:
    case seven_zip::Result::kSuccess:
    case seven_zip::Result::kMemoryMappingFailed:
    case seven_zip::Result::kNoFilename:
    case seven_zip::Result::kEncryptedHeaders:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

base::File SevenZipDelegateImpl::OnTempFileRequest() {
  base::FilePath temp_path;
  base::File temp_file;
  if (base::CreateTemporaryFileInDir(location_, &temp_path)) {
    temp_file.Initialize(
        temp_path,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
            base::File::FLAG_WRITE | base::File::FLAG_WIN_EXCLUSIVE_READ |
            base::File::FLAG_WIN_EXCLUSIVE_WRITE |
            base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_DELETE_ON_CLOSE |
            base::File::FLAG_WIN_SHARE_DELETE);
  }

  if (!temp_file.IsValid()) {
    error_code_ = ::GetLastError();
    unpack_error_ = UNPACK_CREATE_FILE_ERROR;
  }

  return temp_file;
}

bool SevenZipDelegateImpl::OnDirectory(const seven_zip::EntryInfo& entry) {
  if (!CreateDirectory(location_.Append(entry.file_path))) {
    error_code_ = ::GetLastError();
    unpack_error_ = UNPACK_CREATE_FILE_ERROR;
    return false;
  }

  return true;
}

bool SevenZipDelegateImpl::OnEntry(const seven_zip::EntryInfo& entry,
                                   base::span<uint8_t>& output) {
  if (entry.file_path.ReferencesParent()) {
    PLOG(ERROR) << "Path contains a parent directory traversal which is not "
                   "allowed because it could become a security issue: "
                << entry.file_path;
    unpack_error_ = UNPACK_CREATE_FILE_ERROR;
    return false;
  }

  base::FilePath file_path = location_.Append(entry.file_path);
  if (output_file_)
    *output_file_ = file_path;

  CreateDirectory(file_path.DirName());

  current_file_ =
      base::File(file_path, base::File::FLAG_CREATE_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_WIN_EXCLUSIVE_READ |
                                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                base::File::FLAG_CAN_DELETE_ON_CLOSE |
                                base::File::FLAG_WIN_SHARE_DELETE);
  if (!current_file_.IsValid()) {
    PLOG(ERROR) << "Invalid file";
    error_code_ = ::GetLastError();
    unpack_error_ = UNPACK_CREATE_FILE_ERROR;
    return false;
  }

  // The target file is deleted by default unless extracting succeeds.
  current_file_.DeleteOnClose(true);

  if (entry.file_size > 0) {
    mapped_file_.emplace();
    bool mapped_file_ok = mapped_file_->Initialize(
        current_file_.Duplicate(), {0, static_cast<size_t>(entry.file_size)},
        base::MemoryMappedFile::READ_WRITE_EXTEND);
    if (!mapped_file_ok) {
      PLOG(ERROR) << "Can't map file to memory";
      error_code_ = ::GetLastError();
      unpack_error_ = UNPACK_ALLOCATE_ERROR;
      return false;
    }

    output = base::span<uint8_t>(mapped_file_->data(), mapped_file_->length());
  } else {
    output = base::span<uint8_t>();
  }

  // Clear the last error code before the entry is extracted to reduce the
  // likelihood that it will hold an unrelated error code in case extraction
  // fails.
  ::SetLastError(ERROR_SUCCESS);

  return true;
}

bool SevenZipDelegateImpl::EntryDone(seven_zip::Result result,
                                     const seven_zip::EntryInfo& entry) {
  // Take ownership of `current_file_` so that it is always closed when this
  // function exits.
  base::File current_file = std::move(current_file_);

  if (result != seven_zip::Result::kSuccess) {
    auto error_code = ::GetLastError();
    if (error_code != ERROR_SUCCESS)
      error_code_ = error_code;

    switch (result) {
      case seven_zip::Result::kSuccess:
        NOTREACHED_IN_MIGRATION();
        break;
      case seven_zip::Result::kFailedToAllocate:
        unpack_error_ = UNPACK_ALLOCATE_ERROR;
        break;
      case seven_zip::Result::kIoError:
        unpack_error_ = UNPACK_IO_DEVICE_ERROR;
        break;
      case seven_zip::Result::kDiskFull:
        unpack_error_ = UNPACK_DISK_FULL;
        break;
      case seven_zip::Result::kNoFilename:
        LOG(ERROR) << "Couldn't get file name";
        unpack_error_ = UNPACK_NO_FILENAME_ERROR;
        break;
      case seven_zip::Result::kUnknownError:
      case seven_zip::Result::kBadCrc:
      case seven_zip::Result::kMemoryMappingFailed:
      case seven_zip::Result::kMalformedArchive:
      case seven_zip::Result::kUnsupported:
      case seven_zip::Result::kEncryptedHeaders:
        unpack_error_ = UNPACK_EXTRACT_ERROR;
        break;
    }

    mapped_file_.reset();
    return false;
  }

  if (mapped_file_) {
    // Modified pages are not written to disk until they're evicted from the
    // working set. Explicitly kick off the write to disk now
    // (asynchronously) to improve the odds that the file's contents are
    // on-disk when another process (such as chrome.exe) would like to use
    // them.
    ::FlushViewOfFile(mapped_file_->data(), 0);
    // Unmap the target file from the process's address space.
    mapped_file_.reset();
    // Flush to avoid odd behavior, such as the bug in Windows 7 through
    // Windows 10 1809 for PE files described in
    // https://randomascii.wordpress.com/2018/02/25/compiler-bug-linker-bug-windows-kernel-bug/.
    // We've also observed oddly empty files on other Windows versions, so
    // this is unconditional.
    current_file.Flush();
  }

  // On success, `current_file` is kept.
  current_file.DeleteOnClose(false);

  if (!entry.last_modified_time.is_null()) {
    FILETIME filetime = entry.last_modified_time.ToFileTime();
    // Make a best-effort attempt to set the file time.
    SetFileTime(current_file.GetPlatformFile(), nullptr, nullptr, &filetime);
  }

  return true;
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
    std::optional<DWORD> error_code = lzma_util.GetErrorCode();
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

  SevenZipDelegateImpl delegate(location, output_file);
  std::unique_ptr<seven_zip::SevenZipReader> reader =
      seven_zip::SevenZipReader::Create(archive_file_.Duplicate(), delegate);
  if (reader) {
    reader->Extract();
  }
  error_code_ = delegate.error_code();
  return delegate.unpack_error();
}

void LzmaUtilImpl::CloseArchive() {
  archive_file_.Close();
  error_code_ = std::nullopt;
}
