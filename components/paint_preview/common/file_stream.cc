// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/file_stream.h"

#include <stdint.h>
#include <utility>

namespace paint_preview {

// FileWStream

FileWStream::FileWStream(base::File file)
    : file_(std::move(file)), bytes_written_(0) {
  DCHECK(file_.IsValid());
}
// Close() is called in the destructor of |file_|.
FileWStream::~FileWStream() = default;

bool FileWStream::write(const void* buffer, size_t size) {
  if (!file_.IsValid())
    return false;
  int bytes =
      file_.WriteAtCurrentPos(reinterpret_cast<const char*>(buffer), size);
  if (bytes < 0)
    return false;
  bytes_written_ += bytes;
  if (static_cast<size_t>(bytes) != size)
    return false;
  return true;
}

void FileWStream::flush() {
  if (!file_.IsValid())
    return;
  file_.Flush();
}

size_t FileWStream::bytesWritten() const {
  return bytes_written_;
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
