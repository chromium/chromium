// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/test/win/scoped_executable_files.h"

#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/device_signals/test/win/files_data.h"

namespace device_signals::test {
namespace {

constexpr char kSignedExeFilename[] = "signed.exe";
constexpr char kMultiSignedExeFilename[] = "multi-signed.exe";
constexpr char kMetadataExeFilename[] = "metadata.exe";
constexpr char kEmptyExeFilename[] = "empty.exe";

constexpr char kProductName[] = "Test Product Name";
constexpr char kProductVersion[] = "1.0.0.2";

}  // namespace

ScopedExecutableFiles::ScopedExecutableFiles() {
  CHECK(scoped_dir_.CreateUniqueTempDir());
}

ScopedExecutableFiles::~ScopedExecutableFiles() = default;

base::FilePath ScopedExecutableFiles::GetSignedExePath() {
  return LazilyCreateFile(kSignedExeFilename, kSignedExeBase64);
}

base::FilePath ScopedExecutableFiles::GetMultiSignedExePath() {
  return LazilyCreateFile(kMultiSignedExeFilename, kMultiSignedExeBase64);
}

base::FilePath ScopedExecutableFiles::GetMetadataExePath() {
  return LazilyCreateFile(kMetadataExeFilename, kMetadataExeBase64);
}

base::FilePath ScopedExecutableFiles::GetEmptyExePath() {
  return LazilyCreateFile(kEmptyExeFilename, kEmptyExeBase64);
}

std::string ScopedExecutableFiles::GetMetadataProductName() {
  return kProductName;
}

std::string ScopedExecutableFiles::GetMetadataProductVersion() {
  return kProductVersion;
}

base::FilePath ScopedExecutableFiles::LazilyCreateFile(
    std::string_view file_name,
    std::string_view file_data) {
  auto file_path = scoped_dir_.GetPath().AppendASCII(file_name);
  if (!base::PathExists(file_path)) {
    CHECK(CreateTempFile(file_path, file_data));
  }
  return file_path;
}

bool ScopedExecutableFiles::CreateTempFile(const base::FilePath& file_path,
                                           std::string_view file_data) {
  std::optional<std::vector<uint8_t>> decoded_file_data =
      base::Base64Decode(file_data);
  if (!decoded_file_data) {
    return false;
  }

  return base::WriteFile(file_path, decoded_file_data.value());
}

}  // namespace device_signals::test
