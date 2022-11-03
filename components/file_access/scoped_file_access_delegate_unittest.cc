// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"

#include <memory>
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
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
      base::OnceCallback<void(ScopedFileAccess)> callback) override {}
  void RequestFilesAccessForSystem(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(ScopedFileAccess)> callback) override {}
};
int ScopedFileAccessDelegateTestInstance::instance_counter = 0;

class ScopedFileAccessDelegateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // reset static variables
    ScopedFileAccessDelegate::DeleteInstance();
    ScopedFileAccessDelegateTestInstance::instance_counter = 0;
  }
  void TearDown() override { ScopedFileAccessDelegate::DeleteInstance(); }
};

TEST_F(ScopedFileAccessDelegateTest, GetEmptySingleton) {
  EXPECT_EQ(ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, HasEmptySingleton) {
  EXPECT_EQ(ScopedFileAccessDelegate::HasInstance(), false);
}

TEST_F(ScopedFileAccessDelegateTest, InstanceGetInstance) {
  new ScopedFileAccessDelegateTestInstance();
  EXPECT_NE(ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, InstanceHasInstance) {
  new ScopedFileAccessDelegateTestInstance();
  EXPECT_EQ(ScopedFileAccessDelegate::HasInstance(), true);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceGetInstance) {
  new ScopedFileAccessDelegateTestInstance();
  ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(ScopedFileAccessDelegate::Get(), nullptr);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceHasInstance) {
  new ScopedFileAccessDelegateTestInstance();
  ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(ScopedFileAccessDelegate::HasInstance(), false);
}

TEST_F(ScopedFileAccessDelegateTest, DeleteEmptyInstance) {
  EXPECT_NO_FATAL_FAILURE(ScopedFileAccessDelegate::DeleteInstance());
}

TEST_F(ScopedFileAccessDelegateTest, DeleteInstanceDestruct) {
  new ScopedFileAccessDelegateTestInstance();
  ScopedFileAccessDelegate::DeleteInstance();
  EXPECT_EQ(ScopedFileAccessDelegateTestInstance::instance_counter, 0);
}

TEST_F(ScopedFileAccessDelegateTest, MultiSetInstance) {
  new ScopedFileAccessDelegateTestInstance();
  new ScopedFileAccessDelegateTestInstance();
  EXPECT_EQ(ScopedFileAccessDelegateTestInstance::instance_counter, 1);
}

class MockScopedFileAccessDelegate : public ScopedFileAccessDelegate {
 public:
  MOCK_METHOD(void,
              RequestFilesAccess,
              (const std::vector<base::FilePath>&,
               const GURL&,
               base::OnceCallback<void(file_access::ScopedFileAccess)>),
              (override));
  MOCK_METHOD((void),
              RequestFilesAccessForSystem,
              (const std::vector<base::FilePath>&,
               base::OnceCallback<void(file_access::ScopedFileAccess)>),
              (override));
};

class ScopedFileAccessDelegateTaskTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ScopedFileAccessDelegateTaskTest, AccessScopedExecution) {
  MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(
          [](const std::vector<base::FilePath>& files,
             base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
            std::move(callback).Run(ScopedFileAccess::Allowed());
          });
  base::FilePath path = base::FilePath::FromUTF8Unsafe("/test/path");
  base::MockCallback<base::OnceCallback<int()>> task;
  base::MockCallback<base::OnceCallback<void(int)>> reply;
  EXPECT_CALL(task, Run).WillOnce([]() { return 1234; });
  EXPECT_CALL(reply, Run).WillOnce([](int arg) { EXPECT_EQ(1234, arg); });
  scoped_file_access_delegate.AccessScopedPostTaskAndReplyWithResult(
      path, FROM_HERE, {}, task.Get(), reply.Get());
  task_environment_.RunUntilIdle();
}

}  // namespace file_access
