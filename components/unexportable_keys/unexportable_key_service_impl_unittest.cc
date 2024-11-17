// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr BackgroundTaskPriority kTaskPriority =
    BackgroundTaskPriority::kUserVisible;

}  // namespace

class UnexportableKeyServiceImplTest : public testing::Test {
 public:
  UnexportableKeyServiceImplTest()
      : task_manager_(std::make_unique<UnexportableKeyTaskManager>(
            crypto::UnexportableKeyProvider::Config())),
        service_(std::make_unique<UnexportableKeyServiceImpl>(*task_manager_)) {
  }

  UnexportableKeyServiceImpl& service() { return *service_; }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  void ResetService() {
    task_manager_ = std::make_unique<UnexportableKeyTaskManager>(
        crypto::UnexportableKeyProvider::Config());
    service_ = std::make_unique<UnexportableKeyServiceImpl>(*task_manager_);
  }

  void DisableKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    scoped_key_provider_.emplace<crypto::ScopedNullUnexportableKeyProvider>();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  // Provides a mock key provider by default.
  absl::variant<crypto::ScopedMockUnexportableKeyProvider,
                crypto::ScopedNullUnexportableKeyProvider>
      scoped_key_provider_;
  std::unique_ptr<UnexportableKeyTaskManager> task_manager_;
  std::unique_ptr<UnexportableKeyServiceImpl> service_;
};

TEST_F(UnexportableKeyServiceImplTest, IsUnexportableKeyProviderSupported) {
  EXPECT_TRUE(UnexportableKeyServiceImpl::IsUnexportableKeyProviderSupported(
      crypto::UnexportableKeyProvider::Config()));
  DisableKeyProvider();
  EXPECT_FALSE(UnexportableKeyServiceImpl::IsUnexportableKeyProviderSupported(
      crypto::UnexportableKeyProvider::Config()));

  // Test that the service returns a `ServiceError::kNoKeyProvider` error.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          future.GetCallback());
  EXPECT_EQ(future.Get(), base::unexpected(ServiceError::kNoKeyProvider));
}

TEST_F(UnexportableKeyServiceImplTest, GenerateKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, future.Get());

  // Verify that we can get info about the generated key.
  EXPECT_TRUE(service().GetSubjectPublicKeyInfo(key_id).has_value());
  EXPECT_TRUE(service().GetWrappedKey(key_id).has_value());
  EXPECT_THAT(kAcceptableAlgorithms,
              testing::Contains(service().GetAlgorithm(key_id)));
}

TEST_F(UnexportableKeyServiceImplTest, GenerateKeyMultiplePendingRequests) {
  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>,
             kPendingRequests>
      futures;
  for (auto& future : futures) {
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();

  std::set<UnexportableKeyId> key_ids;
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
    ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, future.Get());
    // Verify that we can get info about the generated key.
    EXPECT_TRUE(service().GetSubjectPublicKeyInfo(key_id).has_value());
    EXPECT_TRUE(service().GetWrappedKey(key_id).has_value());
    key_ids.insert(key_id);
  }

  // All key IDs should be unique.
  EXPECT_EQ(key_ids.size(), kPendingRequests);
}

TEST_F(UnexportableKeyServiceImplTest, GenerateKeyFails) {
  // RSA_PKCS1_SHA1 is not supported by the protocol, so the key generation
  // should fail.
  auto unsupported_algorithm = {crypto::SignatureVerifier::RSA_PKCS1_SHA1};
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(unsupported_algorithm, kTaskPriority,
                                          future.GetCallback());
  RunBackgroundTasks();
  EXPECT_EQ(future.Get(),
            base::unexpected(ServiceError::kAlgorithmNotSupported));
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  ResetService();

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  EXPECT_FALSE(from_wrapped_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(from_wrapped_future.IsReady());
  EXPECT_TRUE(from_wrapped_future.Get().has_value());
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedKeyMultiplePendingRequests) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  ResetService();

  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>,
             kPendingRequests>
      from_wrapped_key_futures;
  for (auto& future : from_wrapped_key_futures) {
    service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                               future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();

  // All callbacks should return the same key ID.
  ServiceErrorOr<UnexportableKeyId> unwrapped_key_id =
      from_wrapped_key_futures[0].Get();
  EXPECT_TRUE(unwrapped_key_id.has_value());
  for (auto& future : from_wrapped_key_futures) {
    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(future.Get(), unwrapped_key_id);
  }
}

// Verify that a `FromWrappedSigningKeySlowlyAsync()` callback is executed
// correctly when it's posted from another `FromWrappedSigningKeySlowlyAsync()`
// callback.
TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedKeyNewRequestFromFailedCallback) {
  std::vector<uint8_t> invalid_wrapped_key = {1, 2, 3};

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>
      inner_request_future;
  service().FromWrappedSigningKeySlowlyAsync(
      invalid_wrapped_key, kTaskPriority,
      base::BindLambdaForTesting(
          [&](ServiceErrorOr<UnexportableKeyId> key_id_or_error) {
            service().FromWrappedSigningKeySlowlyAsync(
                invalid_wrapped_key, kTaskPriority,
                inner_request_future.GetCallback());
          }));
  RunBackgroundTasks();
  EXPECT_TRUE(inner_request_future.IsReady());
  EXPECT_EQ(inner_request_future.Get(),
            base::unexpected(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedKeyMultiplePendingRequestsFail) {
  std::vector<uint8_t> empty_wrapped_key;
  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>,
             kPendingRequests>
      from_wrapped_key_futures;
  for (auto& future : from_wrapped_key_futures) {
    service().FromWrappedSigningKeySlowlyAsync(empty_wrapped_key, kTaskPriority,
                                               future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();

  // All callbacks should return failure.
  for (auto& future : from_wrapped_key_futures) {
    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(future.Get(), base::unexpected(ServiceError::kCryptoApiFailed));
  }
}

TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedKeyReturnsTheSameIdWhenExists) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  // `service()` should return the result immediately.
  EXPECT_TRUE(from_wrapped_future.IsReady());
  // Key IDs should be the same.
  EXPECT_EQ(key_id, from_wrapped_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, Sign) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_TRUE(sign_future.Get().has_value());
}

TEST_F(UnexportableKeyServiceImplTest, NonExistingKeyId) {
  UnexportableKeyId fake_key_id;

  // `service()` does not return any info about non-existing key ID.
  EXPECT_EQ(service().GetSubjectPublicKeyInfo(fake_key_id),
            base::unexpected(ServiceError::kKeyNotFound));
  EXPECT_EQ(service().GetWrappedKey(fake_key_id),
            base::unexpected(ServiceError::kKeyNotFound));

  // `SignSlowlyAsync()` should fail.
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(fake_key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_EQ(sign_future.Get(), base::unexpected(ServiceError::kKeyNotFound));
}

}  // namespace unexportable_keys
