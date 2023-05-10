// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_UTILS_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_UTILS_H_

namespace base {
class FilePath;
}

namespace update_client {

// Returns the absolute path to a test file used by update client unit tests.
// These test files exist in the source tree and are available to tests in
// `//components/test/data/update_client.` `file_name` is the relative name of
// the file in that directory.
[[nodiscard]] base::FilePath GetTestFilePath(const char* file_name);

// Duplicates a file from path GetTestFilePath(file) into the provided
// temp_path. This should be provided by a base::ScopedTempDir. Deletion
// should be handled by the caller.
[[nodiscard]] base::FilePath DuplicateTestFile(const base::FilePath& temp_path,
                                               const char* file);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_UTILS_H_
