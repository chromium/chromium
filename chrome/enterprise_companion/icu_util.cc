// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/icu_util.h"

#include "base/i18n/icu_util.h"
#include "base/logging.h"

#if ENTERPRISE_COMPANION_USE_ICU_DATA_FILE
#include <array>
#include <cstdint>
#include <optional>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/enterprise_companion/icu_file_checksum.h"
#include "chrome/enterprise_companion/installer.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

#endif  // ENTERPRISE_COMPANION_USE_ICU_DATA_FILE

namespace enterprise_companion {
namespace {

#if ENTERPRISE_COMPANION_USE_ICU_DATA_FILE
// Returns the upper-case hex SHA256 checksum of the provided file.
std::optional<std::string> HashFile(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SEQUENTIAL_SCAN);
  if (!file.IsValid()) {
    VLOG(1) << "Failed to open " << path;
    return std::nullopt;
  }

  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  base::HeapArray<uint8_t> buffer = base::HeapArray<uint8_t>::WithSize(1 << 16);
  while (true) {
    std::optional<size_t> bytes_read = file.ReadAtCurrentPos(buffer);
    if (!bytes_read) {
      VLOG(1) << "Failed to read " << path;
      return std::nullopt;
    }
    if (bytes_read == 0) {
      break;
    }
    SHA256_Update(&ctx, buffer.data(), *bytes_read);
  }

  std::array<uint8_t, SHA256_DIGEST_LENGTH> hash;
  SHA256_Final(hash.data(), &ctx);
  return base::HexEncode(hash);
}
#endif  // ENTERPRISE_COMPANION_USE_ICU_DATA_FILE

// Performs ICU initialization at best-effort. In official builds ICU data is
// distributed as a file alongside the application, however this may not be
// available due to external modification (e.g. corruption, antivirus,
// enterprise management tools, adventurous users etc.)
bool TryInitializeICU() {
#if ENTERPRISE_COMPANION_USE_ICU_DATA_FILE
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    VLOG(1) << "Failed to retrieve the current executable's path";
    return false;
  }
  const base::FilePath icu_data_path =
      exe_path.DirName().Append(kIcuDataFileName);
  if (!base::PathExists(icu_data_path)) {
    VLOG(1) << "ICU data file is not present";
    return false;
  }
  std::optional<int64_t> icu_data_size = base::GetFileSize(icu_data_path);
  if (!icu_data_size) {
    VLOG(1) << "Failed to get the size of the ICU data file";
    return false;
  }
  if (icu_data_size != kExpectedIcuFileSize) {
    VLOG(1) << "ICU data file size does not match expectations. Got "
            << *icu_data_size << ", want " << kExpectedIcuFileSize;
    return false;
  }

  std::optional<std::string> icu_data_hash = HashFile(icu_data_path);
  if (!icu_data_hash) {
    VLOG(1) << "Failed to hash ICU data file";
    return false;
  }
  if (icu_data_hash != kExpectedIcuFileChecksum) {
    VLOG(1) << "ICU data file SHA256 does not match expectations. Got "
            << *icu_data_hash << ", want " << kExpectedIcuFileChecksum;
    return false;
  }
#endif  // ENTERPRISE_COMPANION_USE_ICU_DATA_FILE
  // InitializeICU may CHECK, though the conditional returns above try to
  // mitigate this. See https://crbug.com/445616.
  if (!base::i18n::InitializeICU()) {
    VLOG(1) << "Failed to initialize ICU";
    return false;
  }

  return true;
}

}  // namespace

void InitializeICU() {
  static bool attempted = false;
  if (attempted) {
    return;
  }
  if (!TryInitializeICU()) {
    VLOG(1) << "ICU libraries were not initialized. Resolution of proxies "
               "containing non-ASCII Unicode code points may crash "
               "(https://crbug.com/420737997).";
  }
  attempted = true;
}

}  // namespace enterprise_companion
