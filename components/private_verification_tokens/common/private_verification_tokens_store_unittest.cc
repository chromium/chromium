// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace {

class PrivateVerificationTokensStoreTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::ScopedTempDir& TempDir() const { return temp_dir_; }

  base::FilePath DbPath(const base::ScopedTempDir& temp_dir) const {
    return temp_dir.GetPath().Append(
        FILE_PATH_LITERAL("PrivateVerificationTokens"));
  }

  // Creates a store.
  void CreateStore(const base::FilePath& path) {
    store_.reset();
    store_ = PrivateVerificationTokensStore::Create(path);
    ASSERT_THAT(store_, testing::NotNull());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PrivateVerificationTokensStore> store_;
};

TEST_F(PrivateVerificationTokensStoreTest, Create_ValidPath_Success) {
  const base::FilePath database_path = DbPath(TempDir());
  CreateStore(database_path);
  EXPECT_THAT(store_, testing::NotNull());
}

TEST_F(PrivateVerificationTokensStoreTest, Create_EmptyPath_Failure) {
  const base::FilePath database_path;
  ASSERT_TRUE(database_path.empty());
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path);
  EXPECT_THAT(store, testing::IsNull());
}

TEST_F(PrivateVerificationTokensStoreTest, Create_Unused_NoFileCreated) {
  const base::FilePath database_path = DbPath(TempDir());
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path);
  ASSERT_THAT(store, testing::NotNull());
  store.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(database_path));
}

}  // namespace

}  // namespace private_verification_tokens
