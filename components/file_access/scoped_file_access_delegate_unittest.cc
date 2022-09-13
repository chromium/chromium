// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"
#include <memory>
#include "testing/gtest/include/gtest/gtest.h"

namespace file_access {
class ScopedFileAccessDelegateTestInstance : public ScopedFileAccessDelegate {
 public:
  static int instance_counter;
  ScopedFileAccessDelegateTestInstance() { ++instance_counter; }
  ~ScopedFileAccessDelegateTestInstance() override { --instance_counter; }

  // ScopedFileAccessDelegate:
  void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override {}
  void RequestFilesAccessForSystem(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override {}
};
int ScopedFileAccessDelegateTestInstance::instance_counter = 0;
}  // namespace file_access

class ScopedFileAccessDelegateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // reset static variables
    file_access::ScopedFileAccessDelegate::scoped_file_access_delegate_ =
        nullptr;
    file_access::ScopedFileAccessDelegateTestInstance::instance_counter = 0;
  }
};

TEST_F(ScopedFileAccessDelegateTest, GetEmptySingleton) {
  EXPECT_EQ(file_access::ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, HasEmptySingleton) {
  EXPECT_EQ(file_access::ScopedFileAccessDelegate::HasInstance(), false);
}

TEST_F(ScopedFileAccessDelegateTest, InstanceGetInstance) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  EXPECT_NE(file_access::ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, InstanceHasInstance) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  EXPECT_EQ(file_access::ScopedFileAccessDelegate::HasInstance(), true);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceGetInstance) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  file_access::ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(file_access::ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceHasInstance) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  file_access::ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(file_access::ScopedFileAccessDelegate::HasInstance(), false);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteEmptyInstance) {
  EXPECT_NO_FATAL_FAILURE(
      file_access::ScopedFileAccessDelegate::DeleteInstance());
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceDestruct) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  file_access::ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(file_access::ScopedFileAccessDelegateTestInstance::instance_counter,
            0);
}

TEST_F(ScopedFileAccessDelegateTest, MultiSetInstance) {
  new file_access::ScopedFileAccessDelegateTestInstance();
  new file_access::ScopedFileAccessDelegateTestInstance();
  EXPECT_EQ(file_access::ScopedFileAccessDelegateTestInstance::instance_counter,
            1);
}
