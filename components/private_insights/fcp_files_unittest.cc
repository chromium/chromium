// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_files.h"

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

TEST(FcpFilesTest, CreateTempFile) {
  FcpFiles files;
  auto temp_file = files.CreateTempFile("test_prefix", ".tmp");
  ASSERT_TRUE(temp_file.ok());
  base::FilePath path = base::FilePath::FromUTF8Unsafe(*temp_file);
  EXPECT_TRUE(base::PathExists(path));
  base::DeleteFile(path);
}

}  // namespace private_insights
