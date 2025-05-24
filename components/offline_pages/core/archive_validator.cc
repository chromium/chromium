// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_validator.h"

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"

namespace offline_pages {

ArchiveValidator::ArchiveValidator() = default;
ArchiveValidator::~ArchiveValidator() = default;

void ArchiveValidator::Update(base::span<const uint8_t> buffer) {
  hash_.Update(buffer);
}

void ArchiveValidator::Update(std::string_view buffer) {
  hash_.Update(buffer);
}

std::string ArchiveValidator::Finish() {
  std::string result(crypto::hash::kSha256Size, 0);
  hash_.Finish(base::as_writable_byte_span(result));
  return result;
}

// static
std::string ArchiveValidator::ComputeDigest(const base::FilePath& file_path) {
  std::pair<int64_t, std::string> result = GetSizeAndComputeDigest(file_path);
  return result.second;
}

// static
std::pair<int64_t, std::string> ArchiveValidator::GetSizeAndComputeDigest(
    const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::make_pair(0LL, std::string());
  }

  ArchiveValidator archive_validator;

  std::array<uint8_t, 1024> buffer;
  int64_t total_read = 0LL;
  while (true) {
    std::optional<size_t> bytes_read = file.ReadAtCurrentPos(buffer);
    if (!bytes_read.has_value()) {
      return {0LL, std::string()};
    }
    if (bytes_read.value() == 0) {
      return {total_read, archive_validator.Finish()};
    }
    total_read += bytes_read.value();
    archive_validator.Update(base::span(buffer).first(*bytes_read));
  }
}

// static
bool ArchiveValidator::ValidateFile(const base::FilePath& file_path,
                                    int64_t expected_file_size,
                                    const std::string& expected_digest) {
  std::optional<int64_t> actual_file_size = base::GetFileSize(file_path);
  if (!actual_file_size.has_value()) {
    return false;
  }
  if (expected_file_size != actual_file_size.value()) {
    return false;
  }

  std::string actual_digest = ComputeDigest(file_path);
  return expected_digest == actual_digest;
}

}  // namespace offline_pages
