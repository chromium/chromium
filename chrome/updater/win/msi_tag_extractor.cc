// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/msi_tag_extractor.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/win/tag_extractor.h"

namespace updater {

namespace {

constexpr size_t kMaxBufferLength = 81920;  // 80K

// Loads up to the last 80K bytes from `filename`.
std::vector<uint8_t> ReadFileTail(const base::FilePath& filename) {
  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return {};
  }

  const int64_t file_length = file.GetLength();

  int bytes_to_read = kMaxBufferLength;
  if (file_length > static_cast<int64_t>(kMaxBufferLength)) {
    if (file.Seek(base::File::FROM_END, -kMaxBufferLength) !=
        kMaxBufferLength) {
      return {};
    }
  } else {
    bytes_to_read = file_length;
  }

  std::vector<uint8_t> buffer(bytes_to_read + 1);
  const int num_bytes_read =
      file.ReadAtCurrentPos(reinterpret_cast<char*>(&buffer[0]), bytes_to_read);
  if (num_bytes_read != bytes_to_read) {
    return {};
  }

  return buffer;
}

tagging::TagArgs ParseTagBuffer(const std::vector<uint8_t>& tag_buffer) {
  if (tag_buffer.empty()) {
    return {};
  }

  const std::string tag_string =
      ReadTagUtf8(tag_buffer.begin(), tag_buffer.end());
  if (tag_string.empty()) {
    return {};
  }

  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(tag_string, {}, &tag_args);
  if (error != tagging::ErrorCode::kSuccess) {
    return {};
  }
  return tag_args;
}

}  // namespace

tagging::TagArgs ExtractTagArgs(const base::FilePath& filename) {
  return ParseTagBuffer(ReadFileTail(filename));
}

}  // namespace updater
