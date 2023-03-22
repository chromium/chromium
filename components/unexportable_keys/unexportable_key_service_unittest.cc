// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr BackgroundTaskPriority kTaskPriority =
    BackgroundTaskPriority::kUserVisible;

}  // namespace

class UnexportableKeyServiceTest : public testing::Test {
 public:
  UnexportableKeyServiceTest()
      : task_manager_(std::make_unique<UnexportableKeyTaskManager>()),
        service_(std::make_unique<UnexportableKeyService>(*task_manager_)) {}

  UnexportableKeyService& service() { return *service_; }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  void ResetService() {
    task_manager_ = std::make_unique<UnexportableKeyTaskManager>();
    service_ = std::make_unique<UnexportableKeyService>(*task_manager_);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  std::unique_ptr<UnexportableKeyTaskManager> task_manager_;
  std::unique_ptr<UnexportableKeyService> service_;
};

TEST_F(UnexportableKeyServiceTest, GenerateKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  ServiceErrorOr<UnexportableKeyId> key_id = future.Get();
  ASSERT_TRUE(key_id.has_value());

  // Verify that we can get info about the generated key.
  EXPECT_TRUE(service().GetSubjectPublicKeyInfo(*key_id).has_value());
  EXPECT_TRUE(service().GetWrappedKey(*key_id).has_value());
  EXPECT_THAT(kAcceptableAlgorithms,
              testing::Contains(service().GetAlgorithm(*key_id)));
}

TEST_F(UnexportableKeyServiceTest, GenerateKeyMultiplePendingRequests) {
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
    ServiceErrorOr<UnexportableKeyId> key_id = future.Get();
    ASSERT_TRUE(key_id.has_value());
    // Verify that we can get info about the generated key.
    EXPECT_TRUE(service().GetSubjectPublicKeyInfo(*key_id).has_value());
    EXPECT_TRUE(service().GetWrappedKey(*key_id).has_value());
    key_ids.insert(*key_id);
  }

  // All key IDs should be unique.
  EXPECT_EQ(key_ids.size(), kPendingRequests);
}

TEST_F(UnexportableKeyServiceTest, GenerateKeyFails) {
  // RSA is not supported by the mock key provider, so the key generation should
  // fail.
  auto unsupported_algorithm = {crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(unsupported_algorithm, kTaskPriority,
                                          future.GetCallback());
  RunBackgroundTasks();
  EXPECT_EQ(future.Get(),
            base::unexpected(ServiceError::kAlgorithmNotSupported));
}

TEST_F(UnexportableKeyServiceTest, FromWrappedKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  ASSERT_TRUE(key_id.has_value());

  ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      service().GetWrappedKey(*key_id);
  ASSERT_TRUE(wrapped_key.has_value());

  ResetService();

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(*wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  EXPECT_FALSE(from_wrapped_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(from_wrapped_future.IsReady());
  EXPECT_TRUE(from_wrapped_future.Get().has_value());
}

TEST_F(UnexportableKeyServiceTest, FromWrappedKeyMultiplePendingRequests) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  ASSERT_TRUE(key_id.has_value());

  ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      service().GetWrappedKey(*key_id);
  ASSERT_TRUE(wrapped_key.has_value());

  ResetService();

  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>,
             kPendingRequests>
      from_wrapped_key_futures;
  for (auto& future : from_wrapped_key_futures) {
    service().FromWrappedSigningKeySlowlyAsync(*wrapped_key, kTaskPriority,
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

TEST_F(UnexportableKeyServiceTest, FromWrappedKeyMultiplePendingRequestsFail) {
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

TEST_F(UnexportableKeyServiceTest, FromWrappedKeyReturnsTheSameIdWhenExists) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  ASSERT_TRUE(key_id.has_value());

  ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      service().GetWrappedKey(*key_id);
  ASSERT_TRUE(wrapped_key.has_value());

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(*wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  // `service()` should return the result immediately.
  EXPECT_TRUE(from_wrapped_future.IsReady());
  ServiceErrorOr<UnexportableKeyId> unwrapped_key_id =
      from_wrapped_future.Get();
  // Key IDs should be the same.
  EXPECT_EQ(key_id, unwrapped_key_id);
}

TEST_F(UnexportableKeyServiceTest, Sign) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  ASSERT_TRUE(key_id.has_value());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(*key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_TRUE(sign_future.Get().has_value());
}

TEST_F(UnexportableKeyServiceTest, NonExistingKeyId) {
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
