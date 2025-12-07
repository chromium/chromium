// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <variant>

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_provider.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace {
// Result histograms:
constexpr std::string_view kGenerateKeyTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.GenerateKey";
constexpr std::string_view kFromWrappedKeyTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.FromWrappedKey";
constexpr std::string_view kSignTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.Sign";
constexpr std::string_view kDeleteKeyTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.DeleteKey";
constexpr std::string_view kGetAllKeysTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.GetAllKeys";
constexpr std::string_view kDeleteAllKeysTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult.DeleteAllKeys";
// Retries histograms:
constexpr std::string_view kGenerateKeyTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.GenerateKey.Success";
constexpr std::string_view kFromWrappedKeyTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.FromWrappedKey.Success";
constexpr std::string_view kSignTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.Sign.Success";
constexpr std::string_view kDeleteKeyTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.DeleteKey.Success";
constexpr std::string_view kGenerateKeyTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.GenerateKey.Failure";
constexpr std::string_view kFromWrappedKeyTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.FromWrappedKey.Failure";
constexpr std::string_view kSignTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.Sign.Failure";
constexpr std::string_view kDeleteKeyTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.DeleteKey.Failure";
constexpr std::string_view kGetAllKeysTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.GetAllKeys.Success";
constexpr std::string_view kGetAllKeysTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.GetAllKeys.Failure";
constexpr std::string_view kDeleteAllKeysTaskRetriesSuccessHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.DeleteAllKeys.Success";
constexpr std::string_view kDeleteAllKeysTaskRetriesFailureHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.DeleteAllKeys.Failure";
}  // namespace

class UnexportableKeyTaskManagerTest : public testing::Test {
 public:
  UnexportableKeyTaskManagerTest() = default;
  ~UnexportableKeyTaskManagerTest() override = default;

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  UnexportableKeyTaskManager& task_manager() { return task_manager_; }

  ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_.emplace<ScopedMockUnexportableKeyProvider>();
  }

  void DisableKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    scoped_key_provider_.emplace<crypto::ScopedNullUnexportableKeyProvider>();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      // QUEUED - tasks don't run until `RunUntilIdle()` is called.
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  // Provides a fake key provider by default.
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               crypto::ScopedNullUnexportableKeyProvider,
               ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
  UnexportableKeyTaskManager task_manager_;
};

TEST_F(UnexportableKeyTaskManagerTest, GenerateKeyAsync) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};

  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ValueIs(NotNull()));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGenerateKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGenerateKeyTaskRetriesSuccessHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       GenerateKeyAsyncFailureUnsupportedAlgorithm) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  // RSA_PKCS1_SHA1 is not supported by the protocol, so the key generation
  // should fail.
  auto unsupported_algorithm = {crypto::SignatureVerifier::RSA_PKCS1_SHA1};

  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), unsupported_algorithm,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGenerateKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kAlgorithmNotSupported, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGenerateKeyTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, GenerateKeyAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};

  DisableKeyProvider();
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGenerateKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kNoKeyProvider, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGenerateKeyTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, FromWrappedKeyAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                       generate_key_future.Get());
  std::vector<uint8_t> wrapped_key = key->key().GetWrappedKey();

  // Second, unwrap the newly generated key.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      unwrap_key_future;

  task_manager().FromWrappedSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), wrapped_key,
      BackgroundTaskPriority::kBestEffort, unwrap_key_future.GetCallback());
  EXPECT_FALSE(unwrap_key_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(unwrap_key_future.IsReady());
  ASSERT_OK_AND_ASSIGN(auto unwrapped_key, unwrap_key_future.Get());
  EXPECT_NE(unwrapped_key, nullptr);
  // New key should have a unique ID.
  EXPECT_NE(unwrapped_key->id(), key->id());
  // Public key should be the same for both keys.
  EXPECT_EQ(key->key().GetSubjectPublicKeyInfo(),
            unwrapped_key->key().GetSubjectPublicKeyInfo());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kFromWrappedKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kFromWrappedKeyTaskRetriesSuccessHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, FromWrappedKeyAsyncFailureEmptyKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  std::vector<uint8_t> empty_wrapped_key;

  task_manager().FromWrappedSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), empty_wrapped_key,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kFromWrappedKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kCryptoApiFailed, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kFromWrappedKeyTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       FromWrappedKeyAsyncFailureNoKeyProvider) {
  // First, obtain a valid wrapped key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                       generate_key_future.Get());
  std::vector<uint8_t> wrapped_key = key->key().GetWrappedKey();

  // Second, emulate the key provider being not available and check that
  // `FromWrappedSigningKeySlowlyAsync()` returns a corresponding error.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;

  DisableKeyProvider();
  task_manager().FromWrappedSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), wrapped_key,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kFromWrappedKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kNoKeyProvider, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kFromWrappedKeyTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, SignAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(auto key, generate_key_future.Get());

  // Second, sign some data with the key.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  task_manager().SignSlowlyAsync(key, data, BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  ASSERT_OK_AND_ASSIGN(const auto signed_data, sign_future.Get());
  EXPECT_THAT(histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
              ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesSuccessHistogramName),
      ElementsAre(base::Bucket(0, 1)));

  // Also verify that the signature was generated correctly.
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(key->key().Algorithm(), signed_data,
                                  key->key().GetSubjectPublicKeyInfo()));
  verifier.VerifyUpdate(data);
  EXPECT_TRUE(verifier.VerifyFinal());
}

TEST_F(UnexportableKeyTaskManagerTest, SignAsyncNullKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};

  task_manager().SignSlowlyAsync(nullptr, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
  EXPECT_THAT(histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
              ElementsAre(base::Bucket(ServiceError::kKeyNotFound, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, RetrySignAsyncWithSuccess) {
  // Generate and (eventually) use a valid key to make sure the signature
  // verifies correctly.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      /*acceptable_algorithms=*/{crypto::SignatureVerifier::ECDSA_SHA256},
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(auto key, generate_key_future.Get());

  auto mocked_key = std::make_unique<MockUnexportableKey>();
  ON_CALL(*mocked_key, Algorithm())
      .WillByDefault(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::Algorithm));
  ON_CALL(*mocked_key, GetSubjectPublicKeyInfo())
      .WillByDefault(
          Invoke(&key->key(),
                 &crypto::UnexportableSigningKey::GetSubjectPublicKeyInfo));
  const std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  EXPECT_CALL(*mocked_key, SignSlowly(ElementsAreArray(data)))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(std::nullopt))
      .WillOnce(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::SignSlowly));
  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(mocked_key), UnexportableKeyId());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  task_manager().SignSlowlyAsync(ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(sign_future.Get());
  EXPECT_THAT(histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
              ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesSuccessHistogramName),
      ElementsAre(base::Bucket(3, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, RetrySignAsyncWithFailure) {
  auto key = std::make_unique<MockUnexportableKey>();
  std::vector<uint8_t> data = {0, 1, 1, 2, 3, 5, 8};
  EXPECT_CALL(*key, SignSlowly(ElementsAreArray(data)))
      .Times(4)
      .WillRepeatedly(Return(std::nullopt));
  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(key), UnexportableKeyId());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  task_manager().SignSlowlyAsync(ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
              ElementsAre(base::Bucket(ServiceError::kCryptoApiFailed, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(3, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       RetrySignAsyncIfSignatureVerificationFailsWithSuccess) {
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      /*acceptable_algorithms=*/{crypto::SignatureVerifier::ECDSA_SHA256},
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(auto key, generate_key_future.Get());

  auto mocked_key = std::make_unique<MockUnexportableKey>();
  ON_CALL(*mocked_key, Algorithm())
      .WillByDefault(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::Algorithm));
  ON_CALL(*mocked_key, GetSubjectPublicKeyInfo())
      .WillByDefault(
          Invoke(&key->key(),
                 &crypto::UnexportableSigningKey::GetSubjectPublicKeyInfo));
  const std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  EXPECT_CALL(*mocked_key, SignSlowly(ElementsAreArray(data)))
      // Return invalid signature the first time.
      .WillOnce(Return(std::vector<uint8_t>{1, 2, 3}))
      .WillOnce(
          Invoke(&key->key(), &crypto::UnexportableSigningKey::SignSlowly));

  base::HistogramTester histogram_tester;

  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(mocked_key), UnexportableKeyId());
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  task_manager().SignSlowlyAsync(ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_OK(sign_future.Get());
  EXPECT_THAT(histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
              ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesSuccessHistogramName),
      ElementsAre(base::Bucket(1, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       RetrySignAsyncIfSignatureVerificationFailsWithFailure) {
  auto mocked_key = std::make_unique<MockUnexportableKey>();
  ON_CALL(*mocked_key, Algorithm())
      .WillByDefault(Return(crypto::SignatureVerifier::ECDSA_SHA256));
  ON_CALL(*mocked_key, GetSubjectPublicKeyInfo())
      .WillByDefault(Return(std::vector<uint8_t>{7, 7, 7}));
  const std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  EXPECT_CALL(*mocked_key, SignSlowly(ElementsAreArray(data)))
      .Times(4)
      .WillRepeatedly(Return(std::vector<uint8_t>{1, 2, 3}));

  base::HistogramTester histogram_tester;

  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(mocked_key), UnexportableKeyId());
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  task_manager().SignSlowlyAsync(ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kVerifySignatureFailed));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kVerifySignatureFailed, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSignTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(3, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, DeleteKeyAsync) {
  // First, generate a new signing key to get a valid wrapped_key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), supported_algorithm,
      BackgroundTaskPriority::kBestEffort, generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                       generate_key_future.Get());
  std::vector<uint8_t> wrapped_key = key->key().GetWrappedKey();

  // Second, delete the key.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<void>> delete_future;
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteSigningKeySlowly)
      .WillOnce(Return(true));
  task_manager().DeleteSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), std::move(wrapped_key),
      BackgroundTaskPriority::kBestEffort, delete_future.GetCallback());
  EXPECT_FALSE(delete_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_future.IsReady());
  EXPECT_OK(delete_future.Get());
  EXPECT_THAT(histogram_tester.GetAllSamples(kDeleteKeyTaskResultHistogramName),
              ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteKeyTaskRetriesSuccessHistogramName),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, DeleteKeyAsyncFailureCryptoApiFailed) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<void>> delete_future;
  std::vector<uint8_t> wrapped_key = {1, 2, 3};

  // Delete the key, but fail to do so.
  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteSigningKeySlowly)
      .WillOnce(Return(false));
  task_manager().DeleteSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), std::move(wrapped_key),
      BackgroundTaskPriority::kBestEffort, delete_future.GetCallback());
  EXPECT_FALSE(delete_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_future.IsReady());
  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(histogram_tester.GetAllSamples(kDeleteKeyTaskResultHistogramName),
              ElementsAre(base::Bucket(ServiceError::kCryptoApiFailed, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteKeyTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, DeleteKeyAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<void>> delete_future;
  std::vector<uint8_t> wrapped_key = {1, 2, 3};

  DisableKeyProvider();
  task_manager().DeleteSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), std::move(wrapped_key),
      BackgroundTaskPriority::kBestEffort, delete_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  EXPECT_THAT(histogram_tester.GetAllSamples(kDeleteKeyTaskResultHistogramName),
              ElementsAre(base::Bucket(ServiceError::kNoKeyProvider, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteKeyTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       DeleteKeyAsyncFailureOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<void>> delete_future;
  std::vector<uint8_t> wrapped_key = {1, 2, 3};

  task_manager().DeleteSigningKeySlowlyAsync(
      crypto::UnexportableKeyProvider::Config(), std::move(wrapped_key),
      BackgroundTaskPriority::kBestEffort, delete_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteKeyTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kOperationNotSupported, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteKeyTaskRetriesFailureHistogramName),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, DeleteAllKeysAsync) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly())
      .WillOnce(Return(1u));

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  EXPECT_FALSE(delete_all_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_all_future.IsReady());
  EXPECT_THAT(delete_all_future.Get(), ValueIs(1u));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kDeleteAllKeysTaskRetriesSuccessHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       DeleteAllKeysAsyncFailureCryptoApiFailed) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  EXPECT_FALSE(delete_all_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_all_future.IsReady());
  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kCryptoApiFailed, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kDeleteAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest, DeleteAllKeysAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  DisableKeyProvider();
  task_manager().DeleteAllSigningKeysSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kNoKeyProvider, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kDeleteAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       DeleteAllKeysAsyncFailureOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_all_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kDeleteAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kOperationNotSupported, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kDeleteAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncNoKeys) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          std::vector<std::unique_ptr<crypto::UnexportableSigningKey>>()));

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(auto keys, future.Get());
  EXPECT_TRUE(keys.empty());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGetAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(kNoServiceErrorForMetrics, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGetAllKeysTaskRetriesSuccessHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncOneKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::make_unique<MockUnexportableKey>(),
          })));

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(auto keys, future.Get());
  EXPECT_EQ(keys.size(), 1u);
}

TEST_F(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncProviderFails) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGetAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kCryptoApiFailed, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGetAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncNoProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  DisableKeyProvider();

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGetAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kNoKeyProvider, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGetAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_F(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kOperationNotSupported));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kGetAllKeysTaskResultHistogramName),
      ElementsAre(base::Bucket(ServiceError::kOperationNotSupported, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kGetAllKeysTaskRetriesFailureHistogramName),
              ElementsAre(base::Bucket(0, 1)));
}

}  // namespace unexportable_keys
