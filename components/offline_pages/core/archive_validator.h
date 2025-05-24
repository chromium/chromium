// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/hash.h"

namespace base {
class FilePath;
}

namespace offline_pages {

// Used to validate an archive file.
class ArchiveValidator {
 public:
  ArchiveValidator();

  ArchiveValidator(const ArchiveValidator&) = delete;
  ArchiveValidator& operator=(const ArchiveValidator&) = delete;

  virtual ~ArchiveValidator();

  void Update(base::span<const uint8_t> buffer);
  void Update(std::string_view buffer);
  std::string Finish();

  // Computes a SHA256 digest of the specified file. Empty string will be
  // returned if the digest cannot be computed.
  // Note that content:// URI can be passed in |file_path| on Android.
  static std::string ComputeDigest(const base::FilePath& file_path);

  // Retrives the file size and computes a SHA256 digest for the specified file.
  // Pair of 0 and empty string will be returned if size and digest cannot be
  // obtained.
  static std::pair<int64_t, std::string> GetSizeAndComputeDigest(
      const base::FilePath& file_path);

  // Returns true if the specified file has |expected_file_size| and
  // |expected_digest|.
  // Note that content URI can be passed in |file_path| on Android.
  static bool ValidateFile(const base::FilePath& file_path,
                           int64_t expected_file_size,
                           const std::string& expected_digest);

 private:
  crypto::hash::Hasher hash_{crypto::hash::HashKind::kSha256};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_
