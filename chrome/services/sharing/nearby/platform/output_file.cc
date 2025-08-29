// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/output_file.h"

#include "base/containers/span.h"
#include "base/notimplemented.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace nearby::chrome {

OutputFile::OutputFile(base::File file) : file_(std::move(file)) {}

OutputFile::~OutputFile() = default;

Exception OutputFile::Write(const ByteArray& data) {
  if (!file_.IsValid()) {
    return {Exception::kIo};
  }
  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(data))) {
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

void OutputFile::SetLastModifiedTime(absl::Time last_modified_time) {
  // Intentionally left not implemented. Not supported in Chromium Nearby.
  NOTIMPLEMENTED();
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

}  // namespace nearby::chrome
