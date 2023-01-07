// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/shared_file.h"

#include <limits>

#include "base/numerics/safe_math.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/system/file_data_source.h"

namespace web_package {

SharedFile::SharedFile(std::unique_ptr<base::File> file)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      file_(std::move(file)) {}

void SharedFile::DuplicateFile(base::OnceCallback<void(base::File)> callback) {
  DCHECK(file_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::File* file) -> base::File { return file->Duplicate(); },
          file_.get()),
      std::move(callback));
}

base::File* SharedFile::operator->() {
  DCHECK(file_);
  return file_.get();
}

SharedFile::~SharedFile() {
  // Move the last reference to `file_` whose destructor leads to blocking call
  // that is not permitted here.
  // It is important that this runs on the same `base::SequencedTaskRunner` that
  // the callback scheduled in `DuplicateFile` runs on. Otherwise deleting the
  // file might lead to a UAF inside the file duplication callback if that one
  // happened to run after the deletion of the file.
  task_runner_->DeleteSoon(FROM_HERE, std::move(file_));
}

std::unique_ptr<SharedFile::SharedFileDataSource> SharedFile::CreateDataSource(
    uint64_t offset,
    uint64_t length) {
  return std::make_unique<SharedFile::SharedFileDataSource>(this, offset,
                                                            length);
}

SharedFile::SharedFileDataSource::SharedFileDataSource(
    scoped_refptr<SharedFile> file,
    uint64_t offset,
    uint64_t length)
    : file_(std::move(file)), offset_(offset), length_(length) {
  error_ = mojo::FileDataSource::ConvertFileErrorToMojoResult(
      (*file_)->error_details());

  // base::File::Read takes int64_t as an offset. So, offset + length should
  // not overflow in int64_t.
  uint64_t max_offset;
  if (!base::CheckAdd(offset, length).AssignIfValid(&max_offset) ||
      (std::numeric_limits<int64_t>::max() < max_offset)) {
    error_ = MOJO_RESULT_INVALID_ARGUMENT;
  }
}

SharedFile::SharedFileDataSource::~SharedFileDataSource() = default;

uint64_t SharedFile::SharedFileDataSource::GetLength() const {
  return length_;
}

SharedFile::SharedFileDataSource::ReadResult
SharedFile::SharedFileDataSource::Read(uint64_t offset,
                                       base::span<char> buffer) {
  ReadResult result;
  result.result = error_;

  if (length_ < offset)
    result.result = MOJO_RESULT_INVALID_ARGUMENT;

  if (result.result != MOJO_RESULT_OK)
    return result;

  uint64_t readable_size = length_ - offset;
  uint64_t writable_size = buffer.size();
  uint64_t copyable_size =
      std::min(std::min(readable_size, writable_size),
               static_cast<uint64_t>(std::numeric_limits<int>::max()));

  int bytes_read =
      (*file_)->Read(offset_ + offset, buffer.data(), copyable_size);
  if (bytes_read < 0) {
    result.result = mojo::FileDataSource::ConvertFileErrorToMojoResult(
        (*file_)->GetLastFileError());
  } else {
    result.bytes_read = bytes_read;
  }
  return result;
}

}  // namespace web_package
