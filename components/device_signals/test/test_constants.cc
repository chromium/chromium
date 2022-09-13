// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/test/test_constants.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace device_signals::test {

namespace {

constexpr char kProductName[] = "Test Product Name";
constexpr char kProductVersion[] = "1.0.0.2";

}  // namespace

base::FilePath GetTestDataDir() {
  return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
      .AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("device_signals");
}

base::FilePath GetSignedExePath() {
  return GetTestDataDir().AppendASCII("signed.exe");
}

base::FilePath GetMetadataExePath() {
  return GetTestDataDir().AppendASCII("metadata.exe");
}

base::FilePath GetEmptyExePath() {
  return GetTestDataDir().AppendASCII("empty.exe");
}

std::string GetMetadataProductName() {
  return kProductName;
}

std::string GetMetadataProductVersion() {
  return kProductVersion;
}

}  // namespace device_signals::test
