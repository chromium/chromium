// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"

#include <memory>
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
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
  void RequestDefaultFilesAccess(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(ScopedFileAccess)> callback) override {}
  RequestFilesAccessIOCallback CreateFileAccessCallback(
      const GURL& destination) const override {
    return base::DoNothing();
  }
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

TEST_F(ScopedFileAccessDelegateTaskTest, RequestFilesAccessHelper_HasInstance) {
  MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, RequestFilesAccess)
      .WillOnce(
          [](const std::vector<base::FilePath>& files,
             const GURL& destination_url,
             base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
            std::move(callback).Run(ScopedFileAccess::Allowed());
          });
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  RequestFilesAccess({}, GURL(), future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(ScopedFileAccessDelegateTaskTest, RequestFilesAccessHelper_NoInstance) {
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  RequestFilesAccess({}, GURL(), future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(ScopedFileAccessDelegateTaskTest,
       RequestFilesAccessForSystemHelper_HasInstance) {
  MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(
          [](const std::vector<base::FilePath>& files,
             base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
            std::move(callback).Run(ScopedFileAccess::Allowed());
          });
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  RequestFilesAccessForSystem({}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(ScopedFileAccessDelegateTaskTest,
       RequestFilesAccessForSystemHelper_NoInstance) {
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  RequestFilesAccessForSystem({}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(ScopedFileAccessDelegateTaskTest,
       CreateFileAccessCallbackHelper_HasInstance) {
  MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback)
      .WillOnce([](const GURL& destination) {
        return base::BindRepeating(
            [](const GURL& destination,
               const std::vector<base::FilePath>& files,
               base::OnceCallback<void(file_access::ScopedFileAccess)>
                   callback) {
              std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
            },
            destination);
      });
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto cb = CreateFileAccessCallback(GURL());
  cb.Run({}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(ScopedFileAccessDelegateTaskTest,
       CreateFileAccessCallbackHelper_NoInstance) {
  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto cb = CreateFileAccessCallback(GURL());
  cb.Run({}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

}  // namespace file_access
