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
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SizeIs;

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr BackgroundTaskPriority kTaskPriority =
    BackgroundTaskPriority::kUserVisible;
constexpr BackgroundTaskOrigin kTaskOrigin =
    BackgroundTaskOrigin::kDeviceBoundSessionCredentials;

// Spare key pool UMA suffix constants.
constexpr std::string_view kSpareKeyPoolUmaRetrievalResultSuffix =
    "RetrievalResult";
constexpr std::string_view kSpareKeyPoolUmaPoolSizeSuffix = "PoolSize";
constexpr std::string_view kSpareKeyPoolUmaRequestLatencySuffix =
    "RequestLatency";
constexpr std::string_view kSpareKeyPoolUmaGenerateErrorSuffix =
    "GenerateError";
constexpr std::string_view kSpareKeyPoolUmaReplenishmentLatencySuffix =
    "ReplenishmentLatency";

constexpr base::TimeDelta kSpareKeyPoolDelay = base::Minutes(2);

// Generates a histogram name for the spare key pool.
std::string GetSpareKeyPoolHistogramName(std::string_view pool_type,
                                         std::string_view suffix) {
  static constexpr std::string_view kSpareKeyPoolHistogramPrefix =
      "Crypto.UnexportableKeys.SparePool.";
  return base::StrCat({kSpareKeyPoolHistogramPrefix, pool_type, ".", suffix});
}

}  // namespace

class UnexportableKeyServiceImplTest : public testing::Test {
 public:
  UnexportableKeyServiceImpl& service() { return *service_; }
  UnexportableKeyTaskManager& task_manager() { return *task_manager_; }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

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

  crypto::ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<crypto::ScopedMockUnexportableKeyProvider>();
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

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      // QUEUED - tasks don't run until `RunUntilIdle()` is called.
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
  };
  // Provides a fake key provider by default.
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               crypto::ScopedNullUnexportableKeyProvider,
               crypto::ScopedMockUnexportableKeyProvider>
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
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>> future;
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
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, future.Get());

  // Verify that we can get info about the generated key.
  EXPECT_OK(service().GetSubjectPublicKeyInfo(key_id));
  EXPECT_OK(service().GetWrappedKey(key_id));
  EXPECT_THAT(kAcceptableAlgorithms, Contains(service().GetAlgorithm(key_id)));
}

TEST_F(UnexportableKeyServiceImplTest, GenerateKeyMultiplePendingRequests) {
  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>,
             kPendingRequests>
      futures;
  for (auto& future : futures) {
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();

  std::set<UnexportableSigningKeyId> key_ids;
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
    ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, future.Get());
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
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>> future;
  service().GenerateSigningKeySlowlyAsync(unsupported_algorithm, kTaskPriority,
                                          future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  ResetService();

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  EXPECT_FALSE(from_wrapped_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(from_wrapped_future.IsReady());
  EXPECT_OK(from_wrapped_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedKeyMultiplePendingRequests) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  ResetService();

  constexpr size_t kPendingRequests = 5;
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>,
             kPendingRequests>
      from_wrapped_key_futures;
  for (auto& future : from_wrapped_key_futures) {
    service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                               future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();

  // All callbacks should return the same key ID.
  ServiceErrorOr<UnexportableSigningKeyId> unwrapped_key_id =
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

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      inner_request_future;
  service().FromWrappedSigningKeySlowlyAsync(
      invalid_wrapped_key, kTaskPriority,
      base::BindLambdaForTesting(
          [&](ServiceErrorOr<UnexportableSigningKeyId> key_id_or_error) {
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
  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>,
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
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
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

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(key_id));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  // `service()` should return the result immediately.
  EXPECT_TRUE(from_wrapped_future.IsReady());
  // Key IDs should be the same.
  EXPECT_EQ(key_id, from_wrapped_future.Get());
}

#endif  // BUILDFLAG(IS_MAC)

TEST_F(
    UnexportableKeyServiceImplTest,
    FromWrappedSigningKeySlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  auto key_for_from_wrapped =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(),
              FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(from_wrapped_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysForGarbageCollectionSlowlyAsyncStatelessProvider) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysForGarbageCollectionSlowlyAsyncAddsKeysToService) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  auto provider_key =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*provider_key, GetWrappedKey).WillByDefault(Return(kWrappedKey));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  UnexportableSigningKeyId key_id = key_ids[0];

  // The key should be available in the service via sync APIs (checking both
  // maps).
  ASSERT_OK(service().GetWrappedKey(key_id));

  // A subsequent `FromWrappedKey` call should return a DIFFERENT ID after
  // running tasks.
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;

  auto key_for_from_wrapped =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(SwitchToMockKeyProvider().mock(),
              FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));

  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());
  ASSERT_FALSE(from_wrapped_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(from_wrapped_future.IsReady());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId new_key_id,
                       from_wrapped_future.Get());
  EXPECT_NE(new_key_id, key_id);
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedSigningKeyBeforeGetAllKeys) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  crypto::MockUnexportableKeyProvider& mock_provider =
      SwitchToMockKeyProvider().mock();

  // First, `FromWrappedSigningKeySlowly` will be called.
  auto key_for_from_wrapped =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  // Then, `GetAllKeysSlowly` will be called.
  auto key_for_get_all =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                       from_wrapped_future.Get());
  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableSigningKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  EXPECT_NE(key_ids[0], key_id);
}

TEST_F(UnexportableKeyServiceImplTest, GetAllKeysBeforeFromWrappedSigningKey) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  crypto::MockUnexportableKeyProvider& mock_provider =
      SwitchToMockKeyProvider().mock();

  // First, `GetAllKeysSlowly` will be called.
  auto key_for_get_all =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  // Then, `FromWrappedSigningKeySlowlyAsync` will be called.
  auto key_for_from_wrapped =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableSigningKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                       from_wrapped_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  EXPECT_NE(key_ids[0], key_id);
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysBeforeFromWrappedSigningKeyWithDeletion) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  crypto::MockUnexportableKeyProvider& mock_provider =
      SwitchToMockKeyProvider().mock();

  // First, `GetAllKeysSlowly` will be called.
  auto key_for_get_all =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_get_all, GetWrappedKey).WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(key_for_get_all),
          })));
  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;

  // Simulate a scenario where the key is deleted after it's returned by
  // `GetAllKeysSlowly`, but before `FromWrappedSigningKeySlowly`'s
  // handling logic is executed. This should be handled gracefully.
  // It is important that the `GetAllKeys` task is scheduled before the
  // FromWrappedSigningKey task below.
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority,
      base::BindLambdaForTesting(
          [&](ServiceErrorOr<std::vector<UnexportableSigningKeyId>> result) {
            service().DeleteKeysSlowlyAsync(*result, kTaskPriority,
                                            base::DoNothing());
            get_all_keys_future.SetValue(std::move(result));
          }));

  // Then, `FromWrappedSigningKeySlowlyAsync` will be called.
  auto key_for_from_wrapped =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_for_from_wrapped, GetWrappedKey)
      .WillByDefault(Return(kWrappedKey));
  EXPECT_CALL(mock_provider, FromWrappedSigningKeySlowly(Eq(kWrappedKey)))
      .WillOnce(Return(std::move(key_for_from_wrapped)));
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(kWrappedKey, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  RunBackgroundTasks();

  // The promises will still be resolved with the key id, but it is no longer
  // known to the service.
  ASSERT_OK_AND_ASSIGN(std::vector<UnexportableSigningKeyId> key_ids,
                       get_all_keys_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                       from_wrapped_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  EXPECT_NE(key_ids[0], key_id);
  EXPECT_OK(service().GetWrappedKey(key_id));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysForGarbageCollectionSlowlyAsyncPopulatesGCMap) {
  const std::vector<uint8_t> kWrappedKey = {1, 2, 3};
  auto provider_key =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*provider_key, GetWrappedKey).WillByDefault(Return(kWrappedKey));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  UnexportableSigningKeyId key_id = key_ids[0];

  // The key should be available in the service via sync APIs (checking both
  // maps).
  ASSERT_OK(service().GetWrappedKey(key_id));

  // But it should be available for deletion (ExtractKeyFromMaps should find
  // it).
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteKeysSlowly)
      .WillOnce(Return(1));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync({key_id}, kTaskPriority,
                                  delete_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_future.Get(), ValueIs(1u));
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysForGarbageCollectionSlowlyAsyncKeyAlreadyExists) {
  // Generate a key to have it in the service.
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId existing_key_id,
                       generate_future.Get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> wrapped_key,
                       service().GetWrappedKey(existing_key_id));

  // Mock the provider to return the same key.
  auto provider_key =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*provider_key, GetWrappedKey).WillByDefault(Return(wrapped_key));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  // `GetAllKeys` should return a DIFFERENT ID for the existing key.
  ASSERT_OK_AND_ASSIGN(const auto& key_ids, get_all_keys_future.Get());
  ASSERT_THAT(key_ids, SizeIs(1));
  EXPECT_NE(key_ids[0], existing_key_id);
}

TEST_F(UnexportableKeyServiceImplTest,
       GetAllKeysForGarbageCollectionSlowlyAsyncProviderFails) {
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllKeysSlowly())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(
    UnexportableKeyServiceImplTest,
    GetAllKeysForGarbageCollectionSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  auto provider_key =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*provider_key, GetWrappedKey)
      .WillByDefault(Return(std::vector<uint8_t>{1, 2, 3}));

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(provider_key),
          })));

  base::test::TestFuture<ServiceErrorOr<std::vector<UnexportableSigningKeyId>>>
      get_all_keys_future;
  service().GetAllKeysForGarbageCollectionSlowlyAsync(
      kTaskPriority, get_all_keys_future.GetCallback());

  DestroyService();
  RunBackgroundTasks();
  EXPECT_THAT(get_all_keys_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, Sign) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {1, 2, 3};
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_OK(sign_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, SignSlowlyAsyncWithAttestationKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId key_id,
                       generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  service().SignSlowlyAsync(key_id, {1, 2, 3, 4}, kTaskPriority,
                            sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(sign_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest,
       SignSlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

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
  UnexportableSigningKeyId fake_key_id;

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
  auto key_to_generate =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_to_generate, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));
  ON_CALL(*key_to_generate, GetWrappedKey)
      .WillByDefault(Return(std::vector<uint8_t>{0, 0, 1}));
  std::vector<uint8_t> data = {1, 2, 3};
  EXPECT_CALL(*key_to_generate, SignSlowly(ElementsAreArray(data)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(std::nullopt));
  SwitchToMockKeyProvider().AddNextGeneratedSigningKey(
      std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest, SignWithRetry) {
  // The valid key is needed here to make sure the signature verifies correctly.
  scoped_refptr<RefCountedUnexportableSigningKey> key = GenerateSigningKey();

  auto key_to_generate =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
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
  SwitchToMockKeyProvider().AddNextGeneratedSigningKey(
      std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  service().SignSlowlyAsync(key_id, data, kTaskPriority,
                            sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(sign_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, GenerateAttestationKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>> future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId key_id, future.Get());

  // Verify that we can get info about the generated key.
  EXPECT_OK(service().GetSubjectPublicKeyInfo(key_id));
  EXPECT_OK(service().GetWrappedKey(key_id));
  EXPECT_THAT(kAcceptableAlgorithms, Contains(service().GetAlgorithm(key_id)));
}

TEST_F(UnexportableKeyServiceImplTest, FromWrappedAttestationKey) {
  // 1. Generate an attestation key.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId key_id,
                       generate_future.Get());

  std::vector<uint8_t> wrapped_key = *service().GetWrappedKey(key_id);

  // 2. Reset service and reconstruct from wrapped key.
  ResetService();

  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      from_wrapped_future;
  service().FromWrappedAttestationKeySlowlyAsync(
      wrapped_key, kTaskPriority, from_wrapped_future.GetCallback());
  EXPECT_FALSE(from_wrapped_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(from_wrapped_future.IsReady());
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId loaded_key_id,
                       from_wrapped_future.Get());

  // Verify that the reconstructed key has the same wrapped key.
  EXPECT_THAT(service().GetWrappedKey(loaded_key_id), ValueIs(wrapped_key));
}

TEST_F(UnexportableKeyServiceImplTest, Certify) {
  // 1. Generate attestation key and signing key.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation_future.GetCallback());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_signing_future;
  service().GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_signing_future.GetCallback());

  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id,
                       generate_attestation_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId signing_key_id,
                       generate_signing_future.Get());

  // 2. Certify.
  std::vector<uint8_t> challenge = {7, 8, 9};
  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  service().CertifySlowlyAsync(attestation_key_id, signing_key_id, challenge,
                               kTaskPriority, certify_future.GetCallback());
  EXPECT_FALSE(certify_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(certify_future.IsReady());
  ASSERT_OK_AND_ASSIGN(crypto::AttestationStatement result,
                       certify_future.Get());
  EXPECT_EQ(result.format, crypto::AttestationStatement::kTpm);
  EXPECT_FALSE(result.statement.empty());
  EXPECT_FALSE(result.signature.empty());
}

TEST_F(UnexportableKeyServiceImplTest, CertifyAttestationKeyNotFound) {
  // Generate a valid signing key first.
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_signing_future;
  service().GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_signing_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId signing_key_id,
                       generate_signing_future.Get());

  // Make up a random attestation key ID.
  UnexportableAttestationKeyId fake_attestation_key_id;

  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  std::vector<uint8_t> challenge = {7, 8, 9};
  service().CertifySlowlyAsync(fake_attestation_key_id, signing_key_id,
                               challenge, kTaskPriority,
                               certify_future.GetCallback());
  EXPECT_TRUE(certify_future.IsReady());
  EXPECT_THAT(certify_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, CertifySigningKeyNotFound) {
  // Generate a valid attestation key first.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id,
                       generate_attestation_future.Get());

  // Make up a random signing key ID.
  UnexportableSigningKeyId fake_signing_key_id;

  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  std::vector<uint8_t> challenge = {7, 8, 9};
  service().CertifySlowlyAsync(attestation_key_id, fake_signing_key_id,
                               challenge, kTaskPriority,
                               certify_future.GetCallback());
  EXPECT_TRUE(certify_future.IsReady());
  EXPECT_THAT(certify_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, CertifyTypeMismatch) {
  // Generate a valid attestation key, and a valid signing key.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation_future.GetCallback());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_signing_future;
  service().GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_signing_future.GetCallback());

  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id,
                       generate_attestation_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId signing_key_id,
                       generate_signing_future.Get());

  UnexportableAttestationKeyId swapped_attestation_key_id(signing_key_id);
  UnexportableSigningKeyId swapped_signing_key_id(attestation_key_id);

  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  std::vector<uint8_t> challenge = {7, 8, 9};
  service().CertifySlowlyAsync(swapped_attestation_key_id,
                               swapped_signing_key_id, challenge, kTaskPriority,
                               certify_future.GetCallback());
  EXPECT_TRUE(certify_future.IsReady());
  EXPECT_THAT(certify_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, CertifyAttestationKey) {
  // 1. Generate two attestation keys.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation1_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation1_future.GetCallback());
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation2_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation2_future.GetCallback());

  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id1,
                       generate_attestation1_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id2,
                       generate_attestation2_future.Get());

  // 2. Certify one attestation key with the other.
  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  service().CertifySlowlyAsync(attestation_key_id1, attestation_key_id2,
                               {7, 8, 9}, kTaskPriority,
                               certify_future.GetCallback());
  EXPECT_FALSE(certify_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(certify_future.IsReady());
  ASSERT_OK_AND_ASSIGN(crypto::AttestationStatement result,
                       certify_future.Get());
  EXPECT_EQ(result.format, crypto::AttestationStatement::kTpm);
  EXPECT_FALSE(result.statement.empty());
  EXPECT_FALSE(result.signature.empty());
}

TEST_F(UnexportableKeyServiceImplTest, CertifyFailed) {
  auto& scoped_provider = SwitchToMockKeyProvider();

  auto mock_attestation_key =
      std::make_unique<crypto::MockUnexportableAttestationKey>();
  crypto::MockUnexportableAttestationKey* raw_attestation_key =
      mock_attestation_key.get();
  std::vector<uint8_t> attestation_wrapped_key = {1, 2, 3};
  ON_CALL(*mock_attestation_key, GetWrappedKey)
      .WillByDefault(Return(attestation_wrapped_key));
  ON_CALL(*mock_attestation_key, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));

  auto mock_signing_key =
      std::make_unique<crypto::MockUnexportableSigningKey>();
  crypto::MockUnexportableSigningKey* raw_signing_key = mock_signing_key.get();
  std::vector<uint8_t> signing_wrapped_key = {4, 5, 6};
  ON_CALL(*mock_signing_key, GetWrappedKey)
      .WillByDefault(Return(signing_wrapped_key));
  ON_CALL(*mock_signing_key, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));

  scoped_provider.AddNextGeneratedAttestationKey(
      std::move(mock_attestation_key));
  scoped_provider.AddNextGeneratedSigningKey(std::move(mock_signing_key));

  // Generate keys.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation_future.GetCallback());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_signing_future;
  service().GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_signing_future.GetCallback());

  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id,
                       generate_attestation_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId signing_key_id,
                       generate_signing_future.Get());

  // CertifySlowly returns std::nullopt (failed).
  std::vector<uint8_t> challenge = {7, 8, 9};
  EXPECT_CALL(*raw_attestation_key,
              CertifySlowly(Ref(*raw_signing_key), ElementsAreArray(challenge)))
      .WillRepeatedly(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  service().CertifySlowlyAsync(attestation_key_id, signing_key_id, challenge,
                               kTaskPriority, certify_future.GetCallback());
  EXPECT_FALSE(certify_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(certify_future.IsReady());
  EXPECT_THAT(certify_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest, CertifyWithRetry) {
  auto& scoped_provider = SwitchToMockKeyProvider();

  auto mock_attestation_key =
      std::make_unique<crypto::MockUnexportableAttestationKey>();
  crypto::MockUnexportableAttestationKey* raw_attestation_key =
      mock_attestation_key.get();
  std::vector<uint8_t> attestation_wrapped_key = {1, 2, 3};
  ON_CALL(*mock_attestation_key, GetWrappedKey)
      .WillByDefault(Return(attestation_wrapped_key));
  ON_CALL(*mock_attestation_key, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));

  auto mock_signing_key =
      std::make_unique<crypto::MockUnexportableSigningKey>();
  crypto::MockUnexportableSigningKey* raw_signing_key = mock_signing_key.get();
  std::vector<uint8_t> signing_wrapped_key = {4, 5, 6};
  ON_CALL(*mock_signing_key, GetWrappedKey)
      .WillByDefault(Return(signing_wrapped_key));
  ON_CALL(*mock_signing_key, Algorithm)
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));

  scoped_provider.AddNextGeneratedAttestationKey(
      std::move(mock_attestation_key));
  scoped_provider.AddNextGeneratedSigningKey(std::move(mock_signing_key));

  // Generate keys.
  base::test::TestFuture<ServiceErrorOr<UnexportableAttestationKeyId>>
      generate_attestation_future;
  service().GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_attestation_future.GetCallback());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_signing_future;
  service().GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      generate_signing_future.GetCallback());

  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableAttestationKeyId attestation_key_id,
                       generate_attestation_future.Get());
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId signing_key_id,
                       generate_signing_future.Get());

  std::vector<uint8_t> challenge = {7, 8, 9};
  crypto::AttestationStatement attestation_statement{
      .format = crypto::AttestationStatement::Format::kTpm,
      .statement = {0x11, 0x22},
      .signature = {0xaa, 0xbb},
  };

  EXPECT_CALL(*raw_attestation_key,
              CertifySlowly(Ref(*raw_signing_key), ElementsAreArray(challenge)))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(attestation_statement));

  base::test::TestFuture<ServiceErrorOr<crypto::AttestationStatement>>
      certify_future;
  service().CertifySlowlyAsync(attestation_key_id, signing_key_id, challenge,
                               kTaskPriority, certify_future.GetCallback());
  EXPECT_FALSE(certify_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(certify_future.IsReady());
  ASSERT_OK_AND_ASSIGN(crypto::AttestationStatement result,
                       certify_future.Get());
  EXPECT_EQ(result.format, attestation_statement.format);
  EXPECT_EQ(result.statement, attestation_statement.statement);
  EXPECT_EQ(result.signature, attestation_statement.signature);
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeys) {
  crypto::ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate some keys.
  constexpr uint8_t kKeysToGenerate = 3;
  std::vector<crypto::UnexportableSigningKey*> raw_keys;
  std::vector<UnexportableSigningKeyId> key_ids;
  for (uint8_t i = 0; i < kKeysToGenerate; ++i) {
    // Provide a unique wrapped key, so that the keys get unique key ids.
    auto mock_key = std::make_unique<crypto::MockUnexportableSigningKey>();
    ON_CALL(*mock_key, GetWrappedKey).WillByDefault(Return(std::vector{i}));

    raw_keys.push_back(
        scoped_provider.AddNextGeneratedSigningKey(std::move(mock_key)));
    base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
        generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                         generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteKeysSlowly(ElementsAreArray(raw_keys)))
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
  crypto::ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedSigningKey(
      std::make_unique<crypto::MockUnexportableSigningKey>());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  // The key should exist before deletion.
  ASSERT_OK(service().GetWrappedKey(key_id));

  UnexportableSigningKeyId fake_key_id;
  std::vector<UnexportableSigningKeyId> key_ids_to_delete = {key_id,
                                                             fake_key_id};

  // Delete the keys. Only the existing key will be passed to the provider.
  EXPECT_CALL(scoped_provider.mock(), DeleteKeysSlowly(ElementsAre(raw_key)))
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
  UnexportableSigningKeyId fake_key_id;
  std::vector<UnexportableSigningKeyId> key_ids_to_delete = {fake_key_id};

  // The provider should not be called.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteKeysSlowly).Times(0);

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_future;
  service().DeleteKeysSlowlyAsync(key_ids_to_delete, kTaskPriority,
                                  delete_future.GetCallback());
  // The operation is synchronous.
  EXPECT_TRUE(delete_future.IsReady());
  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteKeysProviderFails) {
  crypto::ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedSigningKey(
      std::make_unique<crypto::MockUnexportableSigningKey>());
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  // The key should exist before deletion.
  ASSERT_OK(service().GetWrappedKey(key_id));

  // Try to delete the key.
  EXPECT_CALL(scoped_provider.mock(), DeleteKeysSlowly(ElementsAre(raw_key)))
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
  crypto::ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // Generate a key.
  auto* raw_key = scoped_provider.AddNextGeneratedSigningKey(
      std::make_unique<crypto::MockUnexportableSigningKey>());

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

  // Delete the key.
  EXPECT_CALL(scoped_provider.mock(), DeleteKeysSlowly(ElementsAre(raw_key)))
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

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

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
  std::vector<UnexportableSigningKeyId> key_ids;
  for (size_t i = 0; i < kKeysToGenerate; ++i) {
    base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
        generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                         generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllKeysSlowly)
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
  std::vector<UnexportableSigningKeyId> key_ids;
  for (size_t i = 0; i < kKeysToGenerate; ++i) {
    base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
        generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id,
                         generate_future.Get());
    key_ids.push_back(key_id);
  }

  // Verify all keys exist.
  for (const auto& key_id : key_ids) {
    ASSERT_OK(service().GetWrappedKey(key_id));
  }

  // Delete all keys.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllKeysSlowly)
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

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      from_wrapped_future;
  service().FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                             from_wrapped_future.GetCallback());

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());

  RunBackgroundTasks();
  EXPECT_THAT(from_wrapped_future.Get(),
              ErrorIs(ServiceError::kOperationCancelled));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysWithPendingGenerateKey) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
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
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(generate_future.Get());
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysProviderFails) {
  // Generate a key to make sure there is at least one key to delete from the
  // service's perspective.
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK(generate_future.Get());

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllKeysSlowly())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;
  service().DeleteAllKeysSlowlyAsync(delete_all_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceImplTest, DeleteAllKeysWithPendingSign) {
  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());

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
  auto key_to_generate =
      std::make_unique<NiceMock<crypto::MockUnexportableSigningKey>>();
  ON_CALL(*key_to_generate, GetCreationTime)
      .WillByDefault(Return(base::Time::Now()));
  SwitchToMockKeyProvider().AddNextGeneratedSigningKey(
      std::move(key_to_generate));

  base::test::TestFuture<ServiceErrorOr<UnexportableSigningKeyId>>
      generate_future;
  service().GenerateSigningKeySlowlyAsync(kAcceptableAlgorithms, kTaskPriority,
                                          generate_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(UnexportableSigningKeyId key_id, generate_future.Get());
  EXPECT_EQ(service().GetCreationTime(key_id), base::Time::Now());
}

// A value-parameterized test fixture to run the spare key pool tests
// for both "Signing" and "Attestation" key types.
template <typename KeyIdType>
class SpareKeyPoolTest : public UnexportableKeyServiceImplTest {
 protected:
  static constexpr std::string_view pool_type() {
    return std::same_as<KeyIdType, UnexportableSigningKeyId> ? "Signing"
                                                             : "Attestation";
  }

  // Triggers a key generation request and returns a future.
  base::test::TestFuture<ServiceErrorOr<KeyIdType>> GenerateKey(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms = kAcceptableAlgorithms) {
    base::test::TestFuture<ServiceErrorOr<KeyIdType>> future;
    if constexpr (std::same_as<KeyIdType, UnexportableSigningKeyId>) {
      this->service().GenerateSigningKeySlowlyAsync(
          acceptable_algorithms, kTaskPriority, future.GetCallback());
    } else if constexpr (std::same_as<KeyIdType,
                                      UnexportableAttestationKeyId>) {
      this->service().GenerateAttestationKeySlowlyAsync(
          acceptable_algorithms, kTaskPriority, future.GetCallback());
    }
    return future;
  }

  // Sets up the mock provider to generate keys for the correct type.
  void SetupMockKeyGenerator(
      crypto::MockUnexportableKeyProvider& mock_provider) {
    if constexpr (std::same_as<KeyIdType, UnexportableAttestationKeyId>) {
      EXPECT_CALL(mock_provider, GenerateAttestationKeySlowly)
          .WillRepeatedly(
              [](base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
                     acceptable_algorithms) {
                auto key = std::make_unique<
                    NiceMock<crypto::MockUnexportableAttestationKey>>();
                ON_CALL(*key, Algorithm)
                    .WillByDefault(
                        Return(crypto::SignatureVerifier::SignatureAlgorithm::
                                   ECDSA_SHA256));
                static std::atomic<uint8_t> id{0};
                ON_CALL(*key, GetWrappedKey)
                    .WillByDefault(Return(std::vector<uint8_t>{id++}));
                return key;
              });
    } else {
      EXPECT_CALL(mock_provider, GenerateSigningKeySlowly)
          .WillRepeatedly(
              [](base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
                     acceptable_algorithms) {
                auto key = std::make_unique<
                    NiceMock<crypto::MockUnexportableSigningKey>>();
                ON_CALL(*key, Algorithm)
                    .WillByDefault(
                        Return(crypto::SignatureVerifier::SignatureAlgorithm::
                                   ECDSA_SHA256));
                static std::atomic<uint8_t> id{0};
                ON_CALL(*key, GetWrappedKey)
                    .WillByDefault(Return(std::vector<uint8_t>{id++}));
                return key;
              });
    }
  }

  // Sets up the mock provider to return failure for replenishment tasks.
  void SetupFailingKeyGenerator(
      crypto::MockUnexportableKeyProvider& mock_provider) {
    static constexpr size_t kExpectedReplenishmentAttempts = 2;
    if constexpr (std::same_as<KeyIdType, UnexportableAttestationKeyId>) {
      EXPECT_CALL(mock_provider, GenerateAttestationKeySlowly)
          .Times(kExpectedReplenishmentAttempts)
          .WillRepeatedly(
              [](base::span<
                  const crypto::SignatureVerifier::SignatureAlgorithm>)
                  -> std::unique_ptr<crypto::UnexportableAttestationKey> {
                return nullptr;
              });
    } else {
      EXPECT_CALL(mock_provider, GenerateSigningKeySlowly)
          .Times(kExpectedReplenishmentAttempts)
          .WillRepeatedly(
              [](base::span<
                  const crypto::SignatureVerifier::SignatureAlgorithm>)
                  -> std::unique_ptr<crypto::UnexportableSigningKey> {
                return nullptr;
              });
    }
  }
};

using SpareKeyPoolTestTypes =
    ::testing::Types<UnexportableSigningKeyId, UnexportableAttestationKeyId>;
TYPED_TEST_SUITE(SpareKeyPoolTest, SpareKeyPoolTestTypes);

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolCapacityLimits) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // The spare key pool has a strict capacity limit of 2 keys.
  // Fast forward by kSpareKeyPoolDelay to let the pool replenish.
  this->FastForwardBy(kSpareKeyPoolDelay);
  this->RunBackgroundTasks();

  // Request the first key. Since the pool was replenished to capacity 2,
  // this should be an instant cache hit and resolve synchronously.
  auto f1 = this->GenerateKey();
  EXPECT_OK(f1.Get());

  // Request the second key. This should also be an instant cache hit.
  auto f2 = this->GenerateKey();
  EXPECT_OK(f2.Get());

  // Request the third key. Because the pool capacity was strictly 2,
  // the cache is now completely empty and it will fall back to a background
  // task.
  auto f3 = this->GenerateKey();
  EXPECT_FALSE(f3.IsReady());
  this->RunBackgroundTasks();
  EXPECT_OK(f3.Get());

  // Verify UMA histograms.
  // Chronological trace of the pool size at each request:
  // 1. First request (f1): The pool has been fully replenished to capacity 2.
  //    Logs PoolSize = 2. The key is popped, reducing the pool size to 1.
  // 2. Second request (f2): The pool has 1 key remaining.
  //    Logs PoolSize = 1. The key is popped, reducing the pool size to 0.
  // 3. Third request (f3): The pool is completely empty (size 0).
  //    Logs PoolSize = 0. This results in a cache miss and falls back to a
  //    background task.
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      3);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      2, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      1, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0, 1);

  // Verify retrieval results.
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      3);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kHit, 2);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissDidNotReplenishFromLastUse, 1);

  // Verify actual latency values: all requests (hits and misses) execute
  // instantaneously in mock time (0ms).
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 3);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      3);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolMiss) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  auto future = this->GenerateKey();

  // Because the cache is empty, the generation falls back to the ThreadPool.
  // Since the ThreadPool is paused in our test environment, the future must
  // strictly remain pending. This mathematically proves the cache miss.
  EXPECT_FALSE(future.IsReady());

  this->RunBackgroundTasks();

  EXPECT_OK(future.Get());
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissNotInitialized, 1);
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0, 1);
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 1);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      1);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolMissNoKeyForAlgorithm) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // Populate the pool so that total_pool_size > 0.
  this->FastForwardBy(kSpareKeyPoolDelay);
  this->RunBackgroundTasks();

  auto future = this->GenerateKey(
      {crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1});

  this->RunBackgroundTasks();

  // RSA_PKCS1_SHA1 is not supported by the provider, so the key generation
  // should fail.
  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));

  // Verify that the retrieval result logs that the algorithm is not supported
  // by the hardware, and that the request latency is recorded.
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kAlgorithmNotSupported, 1);
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      2, 1);
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 1);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      1);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolMissNoKeyForAlgorithmButPoolNotEmpty) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // Use Mock provider to force replenishment to only generate ECDSA keys.
  crypto::MockUnexportableKeyProvider& mock_provider =
      this->SwitchToMockKeyProvider().mock();
  this->SetupMockKeyGenerator(mock_provider);

  // Trigger replenishment to fill the pool with ECDSA keys.
  this->FastForwardBy(kSpareKeyPoolDelay);
  this->RunBackgroundTasks();

  // Request an RSA key.
  // The provider supports both, so SelectAlgorithm will succeed, but the pool
  // has no RSA keys.
  auto future = this->GenerateKey(
      {crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256});

  // Since it's a miss, it falls back to slow path.
  EXPECT_FALSE(future.IsReady());

  this->RunBackgroundTasks();
  EXPECT_OK(future.Get());

  // Verify telemetry:
  // We should see a miss due to kMissNoKeyForAlgorithm because the pool was
  // NOT empty (it had ECDSA keys), but had no RSA keys.
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissNoKeyForAlgorithm, 1);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolHit) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // Fast forward by kSpareKeyPoolDelay to trigger the initial pool
  // replenishment and populate the cache.
  this->FastForwardBy(kSpareKeyPoolDelay);
  this->RunBackgroundTasks();

  // Request first key. It should hit the cache and succeed synchronously.
  auto f1 = this->GenerateKey();
  EXPECT_OK(f1.Get());

  // Request second key. It should also hit the cache since capacity is 2.
  auto f2 = this->GenerateKey();
  EXPECT_OK(f2.Get());

  // Request third key. Cache is exhausted (size 2). Should fallback to slow
  // path.
  auto f3 = this->GenerateKey();

  // Run background tasks. This will allow the third key generation to complete
  // and will also allow the background replenishment tasks to run.
  this->RunBackgroundTasks();

  EXPECT_OK(f3.Get());

  // Request fourth key. It should hit the replenished cache synchronously.
  auto f4 = this->GenerateKey();
  EXPECT_OK(f4.Get());

  // Run pending background replenishment tasks to avoid a dangling pointer
  // crash during TaskEnvironment teardown.
  this->RunBackgroundTasks();

  // Verify retrieval results.
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kHit, 3);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissDidNotReplenishFromLastUse, 1);

  // Verify PoolSize:
  // f1 saw 2, f2 saw 1, f3 saw 0, f4 saw 2 (fully replenished).
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      4);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      2, 2);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      1, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0, 1);

  // Verify actual latency values: all requests (hits and misses) execute
  // instantaneously in mock time (0ms).
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 4);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      4);

  // Verify replenishment latency: 5 successful replenishment tasks completed
  // and all took exactly 0ms in mock time.
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaReplenishmentLatencySuffix),
      0, 5);

  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      kNoServiceErrorForMetrics, 5);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolReplenishmentFailsAndRecovers) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // We must use a Mock provider here because the standard Fake provider always
  // succeeds and cannot simulate asynchronous hardware generation failures,
  // which is mathematically required to test the failure recovery path.
  // Break the provider so that background replenishment fails.
  crypto::MockUnexportableKeyProvider& mock_provider =
      this->SwitchToMockKeyProvider().mock();
  this->SetupFailingKeyGenerator(mock_provider);

  // Fast forward by 2 minutes to trigger the initial pool replenishment.
  // The generation tasks will fail.
  this->FastForwardBy(kSpareKeyPoolDelay);
  this->RunBackgroundTasks();

  // Restore the provider to a working state.
  this->SetupMockKeyGenerator(mock_provider);

  // Now request a key.
  // Because the pool is empty, it misses the cache and falls back to slow path.
  // The slow path will succeed AND trigger background replenishment tasks.
  auto f1 = this->GenerateKey();

  this->RunBackgroundTasks();

  EXPECT_OK(f1.Get());

  // Subsequent requests should now hit the newly replenished cache
  // synchronously!
  auto f2 = this->GenerateKey();
  EXPECT_OK(f2.Get());

  // Run pending background replenishment tasks to avoid a dangling pointer
  // crash during TaskEnvironment teardown.
  this->RunBackgroundTasks();

  // Verify retrieval results.
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissFailedToCreateSpareKey, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kHit, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      ServiceError::kCryptoApiFailed, 2);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      kNoServiceErrorForMetrics, 3);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      5);

  // Verify PoolSize:
  // f1 saw 0 (failed to replenish), f2 saw 2 (successfully replenished).
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      2);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0, 1);
  this->histogram_tester_.ExpectBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      2, 1);

  // Verify actual latency values: all requests (hits and misses) execute
  // instantaneously in mock time (0ms).
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 2);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      2);

  // Verify replenishment latency: 3 successful replenishment tasks completed
  // (the initial 2 failed and should not record latency), all taking 0ms.
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaReplenishmentLatencySuffix),
      0, 3);
}

TYPED_TEST(SpareKeyPoolTest, SpareKeyPoolFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  auto future = this->GenerateKey();
  EXPECT_FALSE(future.IsReady());

  this->RunBackgroundTasks();

  EXPECT_OK(future.Get());

  // Verify that no spare pool telemetry is logged since the pool is disabled.
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      0);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0);
  // RequestLatency is still recorded when the feature is disabled (control
  // group).
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      1);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      0);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaReplenishmentLatencySuffix),
      0);
}

TYPED_TEST(SpareKeyPoolTest,
           GenerateKeySlowlyAsyncCallbackIsCancelledOnServiceDestruction) {
  base::test::ScopedFeatureList feature_list(
      kEnableUnexportableKeysSpareKeyPool);

  this->ResetService();

  // Requesting a key implicitly triggers background spare pool replenishment
  // tasks.
  auto future = this->GenerateKey();

  // Destroying the service cancels the main request and destroys the owned
  // `SpareKeyPoolRequest` objects in the in-flight replenishment pool. This
  // verifies that the background tasks safely no-op (via the requests'
  // WeakPtrs) without a use-after-free crash on service destruction.
  this->DestroyService();
  this->RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kOperationCancelled));

  // Verify UMA histograms.
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRetrievalResultSuffix),
      SpareKeyPoolRetrievalResult::kMissNotInitialized, 1);
  this->histogram_tester_.ExpectUniqueSample(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaPoolSizeSuffix),
      0, 1);
  this->histogram_tester_.ExpectTimeBucketCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      base::TimeDelta(), 1);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaRequestLatencySuffix),
      1);
  this->histogram_tester_.ExpectTotalCount(
      GetSpareKeyPoolHistogramName(this->pool_type(),
                                   kSpareKeyPoolUmaGenerateErrorSuffix),
      0);
}

}  // namespace unexportable_keys
