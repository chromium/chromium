// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr BackgroundTaskPriority kTaskPriority =
    BackgroundTaskPriority::kUserVisible;
constexpr BackgroundTaskOrigin kTaskOrigin =
    BackgroundTaskOrigin::kDeviceBoundSessionCredentials;

}  // namespace

class UnexportableKeyServiceImplTest : public testing::Test {
 public:
  UnexportableKeyServiceImpl& service() { return *service_; }
  UnexportableKeyTaskManager& task_manager() { return *task_manager_; }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  void ResetService(crypto::UnexportableKeyProvider::Config config = {}) {
    task_manager_.emplace();
    service_.emplace(*task_manager_, kTaskOrigin, std::move(config));
  }

  void DestroyService() { service_ = std::nullopt; }

  void DisableKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    scoped_key_provider_.emplace<crypto::ScopedNullUnexportableKeyProvider>();
  }

  ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_.emplace<ScopedMockUnexportableKeyProvider>();
  }

  // Generates a signing key and returns it. This key is NOT stored in the
  // `service()`. It should be used only in tests where the valid generated key
  // is needed directly.
  scoped_refptr<RefCountedUnexportableSigningKey> GenerateSigningKey() {
    base::test::TestFuture<
        ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
        generate_key_future;
    task_manager_->GenerateSigningKeySlowlyAsync(
        kTaskOrigin, crypto::UnexportableKeyProvider::Config(),
        kAcceptableAlgorithms, BackgroundTaskPriority::kBestEffort,
        generate_key_future.GetCallback());
    RunBackgroundTasks();
    auto key = generate_key_future.Get();
    CHECK(key.has_value());
    return *key;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      // QUEUED - tasks don't run until `RunUntilIdle()` is called.
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
  };
  // Provides a fake key provider by default.
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               crypto::ScopedNullUnexportableKeyProvider,
               ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
  std::optional<UnexportableKeyTaskManager> task_manager_{std::in_place};
  std::optional<UnexportableKeyServiceImpl> service_{
      std::in_place, *task_manager_, kTaskOrigin,
      crypto::UnexportableKeyProvider::Config()};
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
  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
}

TEST_F(UnexportableKeyServiceImplTest,
       IsStatefulUnexportableKeyProviderSupported) {
  EXPECT_FALSE(
      UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
          crypto::UnexportableKeyProvider::Config()));

  // Test that the service returns a `ServiceError::kOperationNotSupported`
  // error.
  base::test::TestFuture<ServiceErrorOr<size_t>> future;
  service().DeleteAllKeysSlowlyAsync(future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kOperationNotSupported));

  SwitchToMockKeyProvider();
  EXPECT_TRUE(
      UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
          crypto::UnexportableKeyProvider::Config()));
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
  EXPECT_OK(service().GetSubjectPublicKeyInfo(key_id));
  EXPECT_OK(service().GetWrappedKey(key_id));
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
    EXPECT_OK(service().GetSubjectPublicKeyInfo(key_id));
    EXPECT_OK(service().GetWrappedKey(key_id));
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
  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));
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
  EXPECT_OK(from_wrapped_future.Get());
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
  EXPECT_OK(unwrapped_key_id);
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
  EXPECT_THAT(inner_request_future.Get(),
              ErrorIs(ServiceError::kCryptoApiFailed));
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
    EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
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

#if BUILDFLAG(IS_MAC)
TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedKeyReturnsTheSameIdWhenExistsWithTaggedConfig) {
  ResetService(/*config=*/{.application_tag = "TagA"});

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

TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedSigningKeySelectsCorrectKeyWithTag) {
  // Re-initialize the service with a specific application tag.
  ResetService(/*config=*/{.application_tag = "TagA"});

  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};

  // Mock the provider to return two keys with the same wrapped key but
  // different tags. This simulates a state where keys from different profiles
  // (or distinct tags) are present in the underlying storage.
  auto key_a = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_a, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  ON_CALL(*key_a, GetKeyTag).WillByDefault(Return("TagA"));

  auto key_b = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_b, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  ON_CALL(*key_b, GetKeyTag).WillByDefault(Return("TagB"));

  // Load all keys into the service.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_a),
              std::move(key_b),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(auto ids, get_all_future.Get());
  // Verify both keys were loaded.
  ASSERT_THAT(ids, SizeIs(2));

  // Request a key from the wrapped key data.
  // The service should use its config ("TagA") to find the matching key.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId selected_id,
                       from_wrapped_future.Get());

  // Verify the selected key is indeed the one with "TagA".
  EXPECT_THAT(service().GetKeyTag(selected_id), ValueIs("TagA"));
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(
    UnexportableKeyServiceImplTest,
    FromWrappedSigningKeySlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  auto key_for_from_wrapped = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(),
              FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(from_wrapped_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysForGarbageCollectionSlowlyAsyncStatelessProvider) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysForGarbageCollectionSlowlyAsyncAddsKeysToService) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  auto provider_key = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*provider_key, GetWrappedKey).WillByDefault(Return(kWrappedKey));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  UnexportableKeyId key_id = key_ids[0];

  // The key should be available in the service.
  EXPECT_THAT(service().GetWrappedKey(key_id), ValueIs(kWrappedKey));

  // A subsequent `FromWrappedKey` call should return the same ID immediately.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  EXPECT_TRUE(from_wrapped_future.IsReady());
  EXPECT_THAT(from_wrapped_future.Get(), ValueIs(key_id));
}

TEST_F(UnexportableKeyServiceImplTest,
       FromWrappedSigningKeyBeforeGetAllSigningKeys) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  MockUnexportableKeyProvider& mock_provider = SwitchToMockKeyProvider().mock();

  // First, `FromWrappedSigningKeySlowly` will be called.
  auto key_for_from_wrapped = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  // Then, `GetAllSigningKeysSlowly` will be called.
  auto key_for_get_all = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, from_wrapped_future.Get());
  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_THAT(key_ids, ElementsAre(key_id));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysBeforeFromWrappedSigningKey) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  MockUnexportableKeyProvider& mock_provider = SwitchToMockKeyProvider().mock();

  // First, `GetAllSigningKeysSlowly` will be called.
  auto key_for_get_all = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  // Then, `FromWrappedSigningKeySlowlyAsync` will be called.
  auto key_for_from_wrapped = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, from_wrapped_future.Get());
  ASSERT_THAT(key_ids, ElementsAre(key_id));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysBeforeFromWrappedSigningKeyWithDeletion) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  MockUnexportableKeyProvider& mock_provider = SwitchToMockKeyProvider().mock();

  // First, `GetAllSigningKeysSlowly` will be called.
  auto key_for_get_all = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;

  // Simulate a scenario where the key is deleted after it's returned by
  // `GetAllSigningKeysSlowly`, but before `FromWrappedSigningKeySlowly`'s
  // handling logic is executed. This should be handled gracefully.
  // It is important that the GetAllSigningKeys task is scheduled before the
  // FromWrappedSigningKey task below.
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority,
      base::BindLambdaForTesting(
          [&](ServiceErrorOr<std::vector<UnexportableKeyId>> result) {
            service().DeleteKeysSlowlyAsync(*result, kTaskPriority,
                                            base::DoNothing());
            get_all_keys_future.SetValue(std::move(result));
          }));

  // Then, `FromWrappedSigningKeySlowlyAsync` will be called.
  auto key_for_from_wrapped = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  RunBackgroundTasks();

  // The promises will still be resolved with the key id, but it is no longer
  // known to the service.
  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, from_wrapped_future.Get());
  ASSERT_THAT(key_ids, ElementsAre(key_id));
  EXPECT_THAT(service().GetWrappedKey(key_id),
              ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysForGarbageCollectionSlowlyAsyncWithKeyCollisions) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  const std::string kTag1 = "tag1";
  const std::string kTag2 = "tag2";

  // Create two keys with the same wrapped key but different tags.
  auto key1 = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key1, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  ON_CALL(*key1, GetKeyTag).WillByDefault(Return(kTag1));

  auto key2 = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key2, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  ON_CALL(*key2, GetKeyTag).WillByDefault(Return(kTag2));

  // The provider returns both keys.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key1),
              std::move(key2),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());

  // Verify that both keys were imported and assigned different IDs.
  ASSERT_THAT(key_ids, SizeIs(2));
  EXPECT_NE(key_ids[0], key_ids[1]);

  // Verify we can access both keys correctly.
  EXPECT_THAT(service().GetWrappedKey(key_ids[0]), ValueIs(kWrappedKey));
  EXPECT_THAT(service().GetWrappedKey(key_ids[1]), ValueIs(kWrappedKey));

  EXPECT_THAT(service().GetKeyTag(key_ids[0]), ValueIs(kTag1));
  EXPECT_THAT(service().GetKeyTag(key_ids[1]), ValueIs(kTag2));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysForGarbageCollectionSlowlyAsyncKeyAlreadyExists) {
  // Generate a key to have it in the service.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId existing_key_id,
                       generate_future.Get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(existing_key_id));

  // Mock the provider to return the same key.
  auto provider_key = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*provider_key, GetWrappedKey).WillByDefault(Return(wrapped_key));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  // `GetAllSigningKeys` should return the existing key ID.
  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());
  ASSERT_THAT(key_ids, ElementsAre(existing_key_id));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllSigningKeysForGarbageCollectionSlowlyAsyncProviderFails) {
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(
    UnexportableKeyServiceImplTest,
    GetAllSigningKeysForGarbageCollectionSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  auto provider_key = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*provider_key, GetWrappedKey)
      .WillByDefault(Return(std::vector<uint8_t>{1, 2, 3}));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableKeyId>>>
      get_all_keys_future;
  service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
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
  EXPECT_OK(sign_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest,
       SignSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  DestroyService();
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, NonExistingKeyId) {
  UnexportableKeyId fake_key_id;

  // `service()` does not return any info about non-existing key ID.
  EXPECT_THAT(service().GetSubjectPublicKeyInfo(fake_key_id),
              ErrorIs(ServiceError::kKeyNotFound));
  EXPECT_THAT(service().GetWrappedKey(fake_key_id),
              ErrorIs(ServiceError::kKeyNotFound));

  // `SignSlowlyAsync()` should fail.
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(fake_key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, SignFailed) {
  auto key_to_generate = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_to_generate, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));
  ON_CALL(*key_to_generate, GetWrappedKey)
      .WillByDefault(Return(std::vector<uint8_t>{0, 0, 1}));
  std::vector<uint8_t> data = {1, 2, 3};
  EXPECT_CALL(*key_to_generate, SignSlowly(ElementsAreArray(data)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(std::nullopt));
  SwitchToMockKeyProvider().AddNextGeneratedKey(std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest, SignWithRetry) {
  // The valid key is needed here to make sure the signature verifies correctly.
  scoped_refptr<RefCountedUnexportableSigningKey> key = GenerateSigningKey();

  auto key_to_generate = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_to_generate, Algorithm)
      .WillByDefault(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::Algorithm));
  ON_CALL(*key_to_generate, GetWrappedKey)
      .WillByDefault(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::GetWrappedKey));
  ON_CALL(*key_to_generate, GetSubjectPublicKeyInfo)
      .WillByDefault(
          Invoke(&key->key(),
                 &crypto::UnexportableSigningKey::GetSubjectPublicKeyInfo));
  const std::vector<uint8_t> data = {1, 2, 3};
  EXPECT_CALL(*key_to_generate, SignSlowly(ElementsAreArray(data)))
      .WillOnce(Return(std::nullopt))
      .WillOnce(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::SignSlowly));
  SwitchToMockKeyProvider().AddNextGeneratedKey(std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(sign_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeys) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate some keys.
  constexpr uint8_t kKeysToGenerate = 3;
  std::vector<crypto::UnexportableSigningKey*> raw_keys;
  std::vector<UnexportableKeyId> key_ids;
  for (uint8_t i = 0; i < kKeysToGenerate; ++i) {
    // Provide a unique wrapped key, so that the keys get unique key ids.
    auto mock_key = std::make_unique<MockUnexportableKey>();
    ON_CALL(*mock_key, GetWrappedKey).WillByDefault(Return(std::vector{i}));

    raw_keys.push_back(
        scoped_provider.AddNextGeneratedKey(std::move(mock_key)));
    base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAreArray(raw_keys)))
      .WillOnce(Return(kKeysToGenerate));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync(key_ids, kTaskPriority,
                                  delete_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_future.Get(), ValueIs(kKeysToGenerate));

  // Verify all keys are deleted.
  for (const auto& key_id : key_ids) {
    EXPECT_THAT(service().GetWrappedKey(key_id),
                ErrorIs(ServiceError::kKeyNotFound));
  }
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeysWithNonExistingKey) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedKey(
      std::make_unique<MockUnexportableKey>());
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  // The key should exist before deletion.
  ASSERT_OK(service().GetWrappedKey(key_id));

  UnexportableKeyId fake_key_id;
  std::vector<UnexportableKeyId> key_ids_to_delete = {key_id, fake_key_id};

  // Delete the keys. Only the existing key will be passed to the provider.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAre(raw_key)))
      .WillOnce(Return(1));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync(key_ids_to_delete, kTaskPriority,
                                  delete_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_future.Get(), ValueIs(1u));

  // The existing key should not exist after deletion.
  EXPECT_THAT(service().GetWrappedKey(key_id),
              ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeysOnlyNonExistingKeys) {
  UnexportableKeyId fake_key_id;
  std::vector<UnexportableKeyId> key_ids_to_delete = {fake_key_id};

  // The provider should not be called.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteSigningKeysSlowly)
      .Times(0);

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync(key_ids_to_delete, kTaskPriority,
                                  delete_future.GetCallback());
  // The operation is synchronous.
  EXPECT_TRUE(delete_future.IsReady());
  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeysProviderFails) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedKey(
      std::make_unique<MockUnexportableKey>());
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  // The key should exist before deletion.
  ASSERT_OK(service().GetWrappedKey(key_id));

  // Try to delete the key.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAre(raw_key)))
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync({key_id}, kTaskPriority,
                                  delete_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));

  // The key is removed from the service maps even if the provider failed to
  // delete it.
  EXPECT_THAT(service().GetWrappedKey(key_id),
              ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest,
       DeleteKeysSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedKey(
      std::make_unique<MockUnexportableKey>());

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  // Delete the key.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAre(raw_key)))
      .WillOnce(Return(1));
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync({key_id}, kTaskPriority,
                                  delete_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeysStatelessProvider) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync({key_id}, kTaskPriority,
                                  delete_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));

  // The key is removed from the service maps even if the operation is not
  // supported by the provider.
  EXPECT_THAT(service().GetWrappedKey(key_id),
              ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeys) {
  // Generate some keys.
  constexpr size_t kKeysToGenerate = 3;
  std::vector<UnexportableKeyId> key_ids;
  for (size_t i = 0; i < kKeysToGenerate; ++i) {
    base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly)
      .WillOnce(Return(kKeysToGenerate));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_all_future.Get(), ValueIs(kKeysToGenerate));

  // Verify all keys are deleted.
  for (const auto& key_id : key_ids) {
    EXPECT_THAT(service().GetWrappedKey(key_id),
                ErrorIs(ServiceError::kKeyNotFound));
  }
}

TEST_F(UnexportableKeyServiceImplTest,
       DeleteAllKeysSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  // Generate some keys.
  constexpr size_t kKeysToGenerate = 3;
  std::vector<UnexportableKeyId> key_ids;
  for (size_t i = 0; i < kKeysToGenerate; ++i) {
    base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly)
      .WillOnce(Return(kKeysToGenerate));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(delete_all_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysWithPendingFromWrappedKey) {
  std::vector<uint8_t> wrapped_key =
      GenerateSigningKey()->key().GetWrappedKey();

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());

  RunBackgroundTasks();
  EXPECT_THAT(from_wrapped_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysWithPendingGenerateKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());

  RunBackgroundTasks();

  // The `GenerateSigningKey` task is cancelled by `DeleteAllKeys`.
  EXPECT_THAT(generate_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysStatelessProvider) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_all_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));

  // Service should be usable after deleting all keys.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(generate_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysProviderFails) {
  // Generate a key to make sure there is at least one key to delete from the
  // service's perspective.
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK(generate_future.Get());

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysWithPendingSign) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  // The sign task is now pending in the task manager.
  EXPECT_FALSE(sign_future.IsReady());

  service().DeleteAllKeysSlowlyAsync(base::DoNothing());

  // After deletion, signing with the same key ID should fail synchronously.
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>>
      sign_after_delete_future;
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_after_delete_future.GetCallback());

  EXPECT_TRUE(sign_after_delete_future.IsReady());
  EXPECT_THAT(sign_after_delete_future.Get(),
              ErrorIs(ServiceError::kKeyNotFound));

  // DeleteAllKeys clears the service's key maps synchronously, and cancels
  // pending tasks.
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, GetCreationTimeWithStatefulKey) {
  auto key_to_generate = std::make_unique<NiceMock<MockUnexportableKey>>();
  ON_CALL(*key_to_generate, GetCreationTime)
      .WillByDefault(Return(base::Time::Now()));
  SwitchToMockKeyProvider().AddNextGeneratedKey(std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableKeyId key_id, generate_future.Get());
  EXPECT_EQ(service().GetCreationTime(key_id), base::Time::Now());
}

}  // namespace unexportable_keys
