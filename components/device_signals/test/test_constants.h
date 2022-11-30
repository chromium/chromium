// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_TEST_TEST_CONSTANTS_H_
#define COMPONENTS_DEVICE_SIGNALS_TEST_TEST_CONSTANTS_H_

#include <string>

namespace base {
class FilePath;
}  // namespace base

namespace device_signals::test {

// Returns an absolute path to test data directory containing test resources.
base::FilePath GetTestDataDir();

// Returns an absolute path to the signed.exe file in the test data directory.
base::FilePath GetSignedExePath();

// Returns an absolute path to the multi-signed.exe file in the test data
// directory.
base::FilePath GetMultiSignedExePath();

// Returns an absolute path to the metadata.exe file in the test data directory.
base::FilePath GetMetadataExePath();

// Returns an absolute path to the empty.exe file in the test data directory.
base::FilePath GetEmptyExePath();

// Returns the expected product name of the metadata.exe test file.
std::string GetMetadataProductName();

// Returns the expected product version of the metadata.exe test file.
std::string GetMetadataProductVersion();

// Returns an absolute path to the TestApp.app test bundle.
base::FilePath GetTestBundlePath();

// Returns an absolute path to the TestApp.app's binary path.
base::FilePath GetTestBundleBinaryPath();

// Returns the expected product name of the test bundle.
std::string GetTestBundleProductName();

// Returns the expected product version of the test bundle.
std::string GetTestBundleProductVersion();

// Returns an absolute path to the UnsignedApp.app test bundle.
base::FilePath GetUnsignedBundlePath();

// Returns an absolute path to nothing (no file/directory).
base::FilePath GetUnusedPath();

// Returns an absolute path to a test empty plist.
base::FilePath GetEmptyPlistPath();

// Returns an absolute path to a test plist containing only dictionary items.
base::FilePath GetOnlyDictionaryPlistPath();

// Returns an absolute path to a test plist containing a mix of dictionary and
// array items.
base::FilePath GetMixArrayDictionaryPlistPath();

}  // namespace device_signals::test

#endif  // COMPONENTS_DEVICE_SIGNALS_TEST_TEST_CONSTANTS_H_
