// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/file_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/shared_temp_dir.h"
#include "components/services/filesystem/util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

static_assert(sizeof(off_t) <= sizeof(int64_t), "off_t too big");
static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t too small");

using base::Time;

namespace filesystem {
namespace {

const size_t kMaxReadSize = 1 * 1024 * 1024;  // 1 MB.

}  // namespace

FileImpl::FileImpl(const base::FilePath& path,
                   uint32_t flags,
                   scoped_refptr<SharedTempDir> temp_dir,
                   scoped_refptr<LockTable> lock_table)
    : file_(path, flags),
      path_(path),
      temp_dir_(std::move(temp_dir)),
      lock_table_(std::move(lock_table)) {
  DCHECK(file_.IsValid());
}

FileImpl::FileImpl(const base::FilePath& path,
                   base::File file,
                   scoped_refptr<SharedTempDir> temp_dir,
                   scoped_refptr<LockTable> lock_table)
    : file_(std::move(file)),
      path_(path),
      temp_dir_(std::move(temp_dir)),
      lock_table_(std::move(lock_table)) {
  DCHECK(file_.IsValid());
}

FileImpl::~FileImpl() {
  if (file_.IsValid())
    lock_table_->RemoveFromLockTable(path_);
}

bool FileImpl::IsValid() const {
  return file_.IsValid();
}

#if !defined(OS_FUCHSIA)
base::File::Error FileImpl::RawLockFile() {
  return file_.Lock();
}

base::File::Error FileImpl::RawUnlockFile() {
  return file_.Unlock();
}
#endif  // !OS_FUCHSIA

void FileImpl::Close(CloseCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_));
    return;
  }

  lock_table_->RemoveFromLockTable(path_);
  file_.Close();
  std::move(callback).Run(base::File::Error::FILE_OK);
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Read(uint32_t num_bytes_to_read,
                    int64_t offset,
                    mojom::Whence whence,
                    ReadCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_), base::nullopt);
    return;
  }
  if (num_bytes_to_read > kMaxReadSize) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_INVALID_OPERATION,
                            base::nullopt);
    return;
  }
  base::File::Error error = IsOffsetValid(offset);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, base::nullopt);
    return;
  }
  error = IsWhenceValid(whence);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, base::nullopt);
    return;
  }

  if (file_.Seek(static_cast<base::File::Whence>(whence), offset) == -1) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED,
                            base::nullopt);
    return;
  }

  std::vector<uint8_t> bytes_read(num_bytes_to_read);
  int num_bytes_read = file_.ReadAtCurrentPos(
      reinterpret_cast<char*>(&bytes_read.front()), num_bytes_to_read);
  if (num_bytes_read < 0) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED,
                            base::nullopt);
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_read), num_bytes_to_read);
  bytes_read.resize(static_cast<size_t>(num_bytes_read));
  std::move(callback).Run(base::File::Error::FILE_OK, std::move(bytes_read));
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Write(const std::vector<uint8_t>& bytes_to_write,
                     int64_t offset,
                     mojom::Whence whence,
                     WriteCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_), 0);
    return;
  }
  // Who knows what |write()| would return if the size is that big (and it
  // actually wrote that much).
  if (bytes_to_write.size() >
#if defined(OS_WIN)
      static_cast<size_t>(std::numeric_limits<int>::max())) {
#else
      static_cast<size_t>(std::numeric_limits<ssize_t>::max())) {
#endif
    std::move(callback).Run(base::File::Error::FILE_ERROR_INVALID_OPERATION, 0);
    return;
  }
  base::File::Error error = IsOffsetValid(offset);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, 0);
    return;
  }
  error = IsWhenceValid(whence);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, 0);
    return;
  }

  if (file_.Seek(static_cast<base::File::Whence>(whence), offset) == -1) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, 0);
    return;
  }

  const char* buf = (bytes_to_write.size() > 0)
                        ? reinterpret_cast<const char*>(&bytes_to_write.front())
                        : nullptr;
  int num_bytes_written =
      file_.WriteAtCurrentPos(buf, static_cast<int>(bytes_to_write.size()));
  if (num_bytes_written < 0) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, 0);
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_written),
            std::numeric_limits<uint32_t>::max());
  std::move(callback).Run(base::File::Error::FILE_OK,
                          static_cast<uint32_t>(num_bytes_written));
}

void FileImpl::Tell(TellCallback callback) {
  Seek(0, mojom::Whence::FROM_CURRENT, std::move(callback));
}

void FileImpl::Seek(int64_t offset,
                    mojom::Whence whence,
                    SeekCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_), 0);
    return;
  }
  base::File::Error error = IsOffsetValid(offset);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, 0);
    return;
  }
  error = IsWhenceValid(whence);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, 0);
    return;
  }

  int64_t position =
      file_.Seek(static_cast<base::File::Whence>(whence), offset);
  if (position < 0) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, 0);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK,
                          static_cast<int64_t>(position));
}

void FileImpl::Stat(StatCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_), nullptr);
    return;
  }

  base::File::Info info;
  if (!file_.GetInfo(&info)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, nullptr);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK,
                          MakeFileInformation(info));
}

void FileImpl::Truncate(int64_t size, TruncateCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_));
    return;
  }
  if (size < 0) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  base::File::Error error = IsOffsetValid(size);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  if (!file_.SetLength(size)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_FOUND);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

void FileImpl::Touch(mojom::TimespecOrNowPtr atime,
                     mojom::TimespecOrNowPtr mtime,
                     TouchCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_));
    return;
  }

  base::Time base_atime = Time::Now();
  if (!atime) {
    base::File::Info info;
    if (!file_.GetInfo(&info)) {
      std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
      return;
    }

    base_atime = info.last_accessed;
  } else if (!atime->now) {
    base_atime = Time::FromDoubleT(atime->seconds);
  }

  base::Time base_mtime = Time::Now();
  if (!mtime) {
    base::File::Info info;
    if (!file_.GetInfo(&info)) {
      std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
      return;
    }

    base_mtime = info.last_modified;
  } else if (!mtime->now) {
    base_mtime = Time::FromDoubleT(mtime->seconds);
  }

  file_.SetTimes(base_atime, base_mtime);
  std::move(callback).Run(base::File::Error::FILE_OK);
}

void FileImpl::Dup(mojo::PendingReceiver<mojom::File> receiver,
                   DupCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_));
    return;
  }

  base::File new_file = file_.Duplicate();
  if (!new_file.IsValid()) {
    std::move(callback).Run(GetError(new_file));
    return;
  }

  if (receiver) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FileImpl>(path_, std::move(new_file), temp_dir_,
                                   lock_table_),
        std::move(receiver));
  }
  std::move(callback).Run(base::File::Error::FILE_OK);
}

void FileImpl::Flush(FlushCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_));
    return;
  }

  bool ret = file_.Flush();
  std::move(callback).Run(ret ? base::File::Error::FILE_OK
                              : base::File::Error::FILE_ERROR_FAILED);
}

void FileImpl::Lock(LockCallback callback) {
  std::move(callback).Run(
      static_cast<base::File::Error>(lock_table_->LockFile(this)));
}

void FileImpl::Unlock(UnlockCallback callback) {
  std::move(callback).Run(
      static_cast<base::File::Error>(lock_table_->UnlockFile(this)));
}

void FileImpl::AsHandle(AsHandleCallback callback) {
  if (!file_.IsValid()) {
    std::move(callback).Run(GetError(file_), base::File());
    return;
  }

  base::File new_file = file_.Duplicate();
  if (!new_file.IsValid()) {
    std::move(callback).Run(GetError(new_file), base::File());
    return;
  }

  base::File::Info info;
  if (!new_file.GetInfo(&info)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, base::File());
    return;
  }

  // Perform one additional check right before we send the file's file
  // descriptor over mojo. This is theoretically redundant, but given that
  // passing a file descriptor to a directory is a sandbox escape on Windows,
  // we should be absolutely paranoid.
  if (info.is_directory) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_A_FILE,
                            base::File());
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK, std::move(new_file));
}

}  // namespace filesystem
