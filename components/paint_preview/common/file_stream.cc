// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/file_stream.h"

#include <stdint.h>
#include <utility>

namespace paint_preview {

namespace {

bool ShouldWrite(size_t current_size, size_t max_size, size_t added_size) {
  // If |current_size| + |added_size| overflow then don't write.
  if (std::numeric_limits<size_t>::max() - current_size < added_size)
    return false;
  return max_size >= current_size + added_size;
}

}  // namespace

// FileWStream

FileWStream::FileWStream(base::File file)
    : file_(std::move(file)),
      max_size_(0),
      bytes_written_(0),
      fake_bytes_written_(0),
      has_write_failed_(false) {
  DCHECK(file_.IsValid());
}

FileWStream::FileWStream(base::File file, size_t max_size)
    : file_(std::move(file)),
      max_size_(max_size),
      bytes_written_(0),
      fake_bytes_written_(0),
      has_write_failed_(false) {
  DCHECK(file_.IsValid());
}

// Close() is called in the destructor of |file_|.
FileWStream::~FileWStream() = default;

bool FileWStream::write(const void* buffer, size_t size) {
  fake_bytes_written_ += size;
  if (!file_.IsValid() || has_write_failed_)
    return false;
  if (max_size_ && !ShouldWrite(bytes_written_, max_size_, size)) {
    has_write_failed_ = true;
    return false;
  }
  int bytes =
      file_.WriteAtCurrentPos(reinterpret_cast<const char*>(buffer), size);
  if (bytes < 0) {
    has_write_failed_ = true;
    return false;
  }
  bytes_written_ += bytes;
  if (static_cast<size_t>(bytes) != size) {
    has_write_failed_ = true;
    return false;
  }
  return true;
}

void FileWStream::flush() {
  if (!file_.IsValid())
    return;
  file_.Flush();
}

size_t FileWStream::bytesWritten() const {
  return fake_bytes_written_;
}

void FileWStream::Close() {
  if (!file_.IsValid())
    return;
  file_.Close();
}

// FileRStream

FileRStream::FileRStream(base::File file)
    : file_(std::move(file)), length_(file_.GetLength()), bytes_read_(0) {
  DCHECK(file_.IsValid());
}
FileRStream::~FileRStream() = default;

size_t FileRStream::read(void* buffer, size_t size) {
  if (!file_.IsValid())
    return 0;
  int64_t num_bytes;
  if (!buffer) {
    int64_t origin = file_.Seek(base::File::FROM_CURRENT, 0);
    if (origin < 0)
      return 0;
    num_bytes = file_.Seek(base::File::FROM_CURRENT, size);
    if (num_bytes < 0)
      return 0;
    num_bytes = num_bytes - origin;
  } else {
    num_bytes = file_.ReadAtCurrentPos(reinterpret_cast<char*>(buffer), size);
  }
  if (num_bytes < 0)
    return 0;
  bytes_read_ += num_bytes;
  return num_bytes;
}

bool FileRStream::isAtEnd() const {
  return bytes_read_ == length_;
}

}  // namespace paint_preview
