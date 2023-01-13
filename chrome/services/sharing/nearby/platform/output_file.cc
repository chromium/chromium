// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/output_file.h"

#include "base/numerics/safe_conversions.h"

namespace nearby {
namespace chrome {

OutputFile::OutputFile(base::File file) : file_(std::move(file)) {}

OutputFile::~OutputFile() = default;

Exception OutputFile::Write(const ByteArray& data) {
  if (!file_.IsValid())
    return {Exception::kIo};

  int bytes_written = file_.WriteAtCurrentPos(data.data(), data.size());
  if (bytes_written != base::checked_cast<int>(data.size()))
    return {Exception::kIo};
  return {Exception::kSuccess};
}

Exception OutputFile::Flush() {
  if (!file_.IsValid())
    return {Exception::kIo};

  if (!file_.Flush())
    return {Exception::kIo};

  return {Exception::kSuccess};
}

Exception OutputFile::Close() {
  if (!file_.IsValid())
    return {Exception::kIo};

  file_.Close();
  return {Exception::kSuccess};
}

}  // namespace chrome
}  // namespace nearby
