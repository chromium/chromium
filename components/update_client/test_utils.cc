// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_utils.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

base::FilePath GetTestFilePath(const char* file_name) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file_name);
}

base::FilePath DuplicateTestFile(const base::FilePath& temp_path,
                                 const char* file) {
  base::FilePath dest_path = temp_path.AppendASCII(file);
  EXPECT_TRUE(base::CreateDirectory(dest_path.DirName()));
  EXPECT_TRUE(base::PathExists(GetTestFilePath(file)));
  EXPECT_TRUE(base::CopyFile(GetTestFilePath(file), dest_path));
  return dest_path;
}

}  // namespace update_client
