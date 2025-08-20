// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/sharing/nearby/platform/input_file.h"

#include <vector>

#include "base/logging.h"

namespace nearby::chrome {

InputFile::InputFile(base::File file) : file_(std::move(file)) {}

InputFile::~InputFile() = default;

std::string InputFile::GetFilePath() const {
  // File path is not supported.
  return std::string();
}

std::int64_t InputFile::GetTotalSize() const {
  if (!file_.IsValid())
    return -1;

  return file_.GetLength();
}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  if (!file_.IsValid())
    return Exception::kIo;

  if (size == 0) {
    return ExceptionOr<ByteArray>(ByteArray());
  }

  std::vector<char> buf(size);
  int num_bytes_read = file_.ReadAtCurrentPos(buf.data(), size);

  if (num_bytes_read < 0 || num_bytes_read > GetTotalSize())
    return Exception::kIo;

  return ExceptionOr<ByteArray>(ByteArray(buf.data(), num_bytes_read));
}

Exception InputFile::Close() {
  if (!file_.IsValid())
    return {Exception::kIo};

  file_.Close();
  return {Exception::kSuccess};
}

base::File InputFile::ExtractUnderlyingFile() {
  return std::move(file_);
}

}  // namespace nearby::chrome
