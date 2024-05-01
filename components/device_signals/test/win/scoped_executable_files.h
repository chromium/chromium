// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_TEST_WIN_SCOPED_EXECUTABLE_FILES_H_
#define COMPONENTS_DEVICE_SIGNALS_TEST_WIN_SCOPED_EXECUTABLE_FILES_H_

#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals::test {

class ScopedExecutableFiles {
 public:
  ScopedExecutableFiles();
  ~ScopedExecutableFiles();

  // Returns an absolute path to the signed.exe file in the test data directory.
  // Lazily creates the file.
  base::FilePath GetSignedExePath();

  // Returns an absolute path to the multi-signed.exe file in the test data
  // directory. Lazily creates the file.
  base::FilePath GetMultiSignedExePath();

  // Returns an absolute path to the metadata.exe file in the test data
  // directory. Lazily creates the file.
  base::FilePath GetMetadataExePath();

  // Returns an absolute path to the empty.exe file in the test data directory.
  // Lazily creates the file.
  base::FilePath GetEmptyExePath();

  // Returns the expected product name of the metadata.exe test file.
  std::string GetMetadataProductName();

  // Returns the expected product version of the metadata.exe test file.
  std::string GetMetadataProductVersion();

 private:
  base::FilePath LazilyCreateFile(std::string_view file_name,
                                  std::string_view file_data);

  bool CreateTempFile(const base::FilePath& file_path,
                      std::string_view file_data);

  // TODO(https://github.com/llvm/llvm-project/issues/61334): Explicit
  // [[maybe_unused]] attribute shouuld not be necessary here.
  [[maybe_unused]] base::ScopedAllowBlockingForTesting allow_blocking_;
  base::ScopedTempDir scoped_dir_;
};

}  // namespace device_signals::test

#endif  // COMPONENTS_DEVICE_SIGNALS_TEST_WIN_SCOPED_EXECUTABLE_FILES_H_
