// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/test/test_constants.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace device_signals::test {

namespace {

constexpr char kTestBundleProductName[] = "TestApp";
constexpr char kTestBundleProductVersion[] = "10.8";

}  // namespace

base::FilePath GetTestDataDir() {
  return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
      .AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("device_signals");
}

base::FilePath GetTestBundlePath() {
  return GetTestDataDir().AppendASCII("TestApp.app");
}

base::FilePath GetTestBundleBinaryPath() {
  return GetTestBundlePath()
      .AppendASCII("Contents")
      .AppendASCII("MacOS")
      .AppendASCII("TestApp");
}

std::string GetTestBundleProductName() {
  return kTestBundleProductName;
}

std::string GetTestBundleProductVersion() {
  return kTestBundleProductVersion;
}

base::FilePath GetUnsignedBundlePath() {
  return GetTestDataDir().AppendASCII("UnsignedApp.app");
}

base::FilePath GetUnusedPath() {
  return GetTestDataDir().AppendASCII("Unused");
}

base::FilePath GetEmptyPlistPath() {
  return GetTestDataDir().AppendASCII("empty.plist");
}

base::FilePath GetMixArrayDictionaryPlistPath() {
  return GetTestDataDir().AppendASCII("mix_array_dictionary.plist");
}

base::FilePath GetOnlyDictionaryPlistPath() {
  return GetTestDataDir().AppendASCII("only_dictionary.plist");
}

}  // namespace device_signals::test
