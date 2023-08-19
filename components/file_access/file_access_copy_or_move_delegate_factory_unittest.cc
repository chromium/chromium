// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/file_access_copy_or_move_delegate_factory.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace file_access {
class FactoryTestInstance : public FileAccessCopyOrMoveDelegateFactory {
 public:
  static int instance_counter;
  FactoryTestInstance() { ++instance_counter; }
  ~FactoryTestInstance() override { --instance_counter; }

  std::unique_ptr<storage::CopyOrMoveHookDelegate> MakeHook() override {
    return nullptr;
  }
};
int FactoryTestInstance::instance_counter = 0;

class FileAccessCopyOrMoveDelegateFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // reset static variables
    FileAccessCopyOrMoveDelegateFactory::
        file_access_copy_or_move_delegate_factory_ = nullptr;
    FactoryTestInstance::instance_counter = 0;
  }
};

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, GetEmptySingleton) {
  EXPECT_EQ(FileAccessCopyOrMoveDelegateFactory::Get(), nullptr);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, HasEmptySingleton) {
  EXPECT_EQ(FileAccessCopyOrMoveDelegateFactory::HasInstance(), false);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, InstanceGetInstance) {
  new FactoryTestInstance();
  EXPECT_NE(FileAccessCopyOrMoveDelegateFactory::Get(), nullptr);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, InstanceHasInstance) {
  new FactoryTestInstance();
  EXPECT_EQ(FileAccessCopyOrMoveDelegateFactory::HasInstance(), true);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, DeleteInstanceGetInstance) {
  new FactoryTestInstance();
  FileAccessCopyOrMoveDelegateFactory::DeleteInstance();
  EXPECT_EQ(FileAccessCopyOrMoveDelegateFactory::Get(), nullptr);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, DeleteInstanceHasInstance) {
  new FactoryTestInstance();
  FileAccessCopyOrMoveDelegateFactory::DeleteInstance();
  EXPECT_EQ(FileAccessCopyOrMoveDelegateFactory::HasInstance(), false);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, DeleteEmptyInstance) {
  EXPECT_NO_FATAL_FAILURE(
      FileAccessCopyOrMoveDelegateFactory::DeleteInstance());
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, DeleteInstanceDestruct) {
  new FactoryTestInstance();
  FileAccessCopyOrMoveDelegateFactory::DeleteInstance();
  EXPECT_EQ(FactoryTestInstance::instance_counter, 0);
}

TEST_F(FileAccessCopyOrMoveDelegateFactoryTest, MultiSetInstance) {
  new FactoryTestInstance();
  new FactoryTestInstance();
  EXPECT_EQ(FactoryTestInstance::instance_counter, 1);
}

}  // namespace file_access
