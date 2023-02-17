// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {
constexpr UnexportableKeyId kTestToken{base::Token(1234, 5678)};
}  // namespace

class UnexportableKeyTaskManagerTest : public testing::Test {
 public:
  UnexportableKeyTaskManagerTest() = default;
  ~UnexportableKeyTaskManagerTest() override = default;

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  UnexportableKeyTaskManager& task_manager() { return task_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  UnexportableKeyTaskManager task_manager_;
};

TEST_F(UnexportableKeyTaskManagerTest, GenerateKeyAsync) {
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Get(), nullptr);
}

TEST_F(UnexportableKeyTaskManagerTest, GenerateKeyAsync_Failure) {
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      future;
  // RSA is not supported by the mock key provider, so the key generation should
  // fail.
  auto unsupported_algorithm = {crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      unsupported_algorithm, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  RunBackgroundTasks();
  EXPECT_EQ(future.Get(), nullptr);
}

TEST_F(UnexportableKeyTaskManagerTest, FromWrappedKeyAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  auto key = generate_key_future.Get();
  std::vector<uint8_t> wrapped_key = key->key().GetWrappedKey();

  // Second, unwrap the newly generated key.
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      unwrap_key_future;
  task_manager().FromWrappedSigningKeySlowlyAsync(
      wrapped_key, kTestToken, BackgroundTaskPriority::kBestEffort,
      unwrap_key_future.GetCallback());
  EXPECT_FALSE(unwrap_key_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(unwrap_key_future.IsReady());
  auto unwrapped_key = unwrap_key_future.Get();
  EXPECT_NE(unwrapped_key, nullptr);
  EXPECT_EQ(unwrapped_key->id(), kTestToken);
  // Public key should be the same for both keys.
  EXPECT_EQ(key->key().GetSubjectPublicKeyInfo(),
            unwrapped_key->key().GetSubjectPublicKeyInfo());
}

TEST_F(UnexportableKeyTaskManagerTest, FromWrappedKeyAsync_Failure) {
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      future;
  std::vector<uint8_t> empty_wrapped_key;
  task_manager().FromWrappedSigningKeySlowlyAsync(
      empty_wrapped_key, kTestToken, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  RunBackgroundTasks();
  EXPECT_EQ(future.Get(), nullptr);
}

TEST_F(UnexportableKeyTaskManagerTest, SignAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<scoped_refptr<RefCountedUnexportableSigningKey>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  auto key = generate_key_future.Get();

  // Second, sign some data with the key.
  base::test::TestFuture<absl::optional<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  task_manager().SignSlowlyAsync(key, data, BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  const auto& signed_data = sign_future.Get();
  ASSERT_TRUE(signed_data.has_value());

  // Also verify that the signature was generated correctly.
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(key->key().Algorithm(), signed_data.value(),
                                  key->key().GetSubjectPublicKeyInfo()));
  verifier.VerifyUpdate(data);
  EXPECT_TRUE(verifier.VerifyFinal());
}

TEST_F(UnexportableKeyTaskManagerTest, SignAsync_NullKey) {
  base::test::TestFuture<absl::optional<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  task_manager().SignSlowlyAsync(nullptr, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(sign_future.Get().has_value());
}

}  // namespace unexportable_keys
