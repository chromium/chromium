// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <memory>
#include <variant>

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_origin.h"
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
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Values;

namespace {

constexpr std::string_view kTaskResultHistogramNameFormat =
    "Crypto.UnexportableKeys.BackgroundTaskResult%s.%s";

constexpr std::string_view kTaskRetriesSuccessHistogramNameFormat =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.%s.Success";
constexpr std::string_view kTaskRetriesFailureHistogramNameFormat =
    "Crypto.UnexportableKeys.BackgroundTaskRetries.%s.Failure";

constexpr std::string_view kGenerateKeyTaskType = "GenerateKey";
constexpr std::string_view kFromWrappedKeyTaskType = "FromWrappedKey";
constexpr std::string_view kSignTaskType = "Sign";
constexpr std::string_view kDeleteKeysTaskType = "DeleteKeys";
constexpr std::string_view kGetAllKeysTaskType = "GetAllKeys";
constexpr std::string_view kDeleteAllKeysTaskType = "DeleteAllKeys";

scoped_refptr<RefCountedUnexportableSigningKey> MakeRefCountedKey(
    base::span<const uint8_t> wrapped_key) {
  auto mock_key = std::make_unique<MockUnexportableKey>();
  ON_CALL(*mock_key, GetWrappedKey)
      .WillByDefault(Return(base::ToVector(wrapped_key)));
  return base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(mock_key), UnexportableKeyId());
}

}  // namespace

struct TestParams {
  BackgroundTaskOrigin origin =
      BackgroundTaskOrigin::kDeviceBoundSessionCredentials;
  std::string expected_origin_suffix;
  std::string test_suffix;
};

class UnexportableKeyTaskManagerTest
    : public testing::TestWithParam<TestParams> {
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

  std::string GetResultHistogramName(std::string_view task_type) {
    return absl::StrFormat(kTaskResultHistogramNameFormat, "", task_type);
  }

  std::string GetResultHistogramWithOriginName(std::string_view task_type) {
    return absl::StrFormat(kTaskResultHistogramNameFormat,
                           GetParam().expected_origin_suffix, task_type);
  }

  void VerifyResultHistograms(const base::HistogramTester& histogram_tester,
                              std::string_view format,
                              ServiceError expected_error) {
    histogram_tester.ExpectUniqueSample(GetResultHistogramName(format),
                                        expected_error,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        GetResultHistogramWithOriginName(format), expected_error,
        /*expected_bucket_count=*/1);
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

TEST_P(UnexportableKeyTaskManagerTest, GenerateKeyAsync) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};

  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ValueIs(NotNull()));
  VerifyResultHistograms(histogram_tester, kGenerateKeyTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesSuccessHistogramNameFormat, kGenerateKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       GenerateKeyAsyncFailureUnsupportedAlgorithm) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  // RSA_PKCS1_SHA1 is not supported by the protocol, so the key generation
  // should fail.
  auto unsupported_algorithm = {crypto::SignatureVerifier::RSA_PKCS1_SHA1};

  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      unsupported_algorithm, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));
  VerifyResultHistograms(histogram_tester, kGenerateKeyTaskType,
                         ServiceError::kAlgorithmNotSupported);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kGenerateKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, GenerateKeyAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};

  DisableKeyProvider();
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  VerifyResultHistograms(histogram_tester, kGenerateKeyTaskType,
                         ServiceError::kNoKeyProvider);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kGenerateKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, FromWrappedKeyAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
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
      GetParam().origin, crypto::UnexportableKeyProvider::Config(), wrapped_key,
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
  VerifyResultHistograms(histogram_tester, kFromWrappedKeyTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesSuccessHistogramNameFormat, kFromWrappedKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, FromWrappedKeyAsyncFailureEmptyKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      future;
  std::vector<uint8_t> empty_wrapped_key;

  task_manager().FromWrappedSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      empty_wrapped_key, BackgroundTaskPriority::kBestEffort,
      future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  VerifyResultHistograms(histogram_tester, kFromWrappedKeyTaskType,
                         ServiceError::kCryptoApiFailed);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kFromWrappedKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       FromWrappedKeyAsyncFailureNoKeyProvider) {
  // First, obtain a valid wrapped key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
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
      GetParam().origin, crypto::UnexportableKeyProvider::Config(), wrapped_key,
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  VerifyResultHistograms(histogram_tester, kFromWrappedKeyTaskType,
                         ServiceError::kNoKeyProvider);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kFromWrappedKeyTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, SignAsync) {
  // First, generate a new signing key.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(auto key, generate_key_future.Get());

  // Second, sign some data with the key.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};
  task_manager().SignSlowlyAsync(GetParam().origin, key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  ASSERT_OK_AND_ASSIGN(const auto signed_data, sign_future.Get());
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(0, 1)));

  // Also verify that the signature was generated correctly.
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(key->key().Algorithm(), signed_data,
                                  key->key().GetSubjectPublicKeyInfo()));
  verifier.VerifyUpdate(data);
  EXPECT_TRUE(verifier.VerifyFinal());
}

TEST_P(UnexportableKeyTaskManagerTest, SignAsyncNullKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data = {4, 8, 15, 16, 23, 42};

  task_manager().SignSlowlyAsync(GetParam().origin, nullptr, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kKeyNotFound));
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         ServiceError::kKeyNotFound);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, RetrySignAsyncWithSuccess) {
  // Generate and (eventually) use a valid key to make sure the signature
  // verifies correctly.
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
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
  task_manager().SignSlowlyAsync(GetParam().origin, ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_OK(sign_future.Get());
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(3, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, RetrySignAsyncWithFailure) {
  auto key = std::make_unique<MockUnexportableKey>();
  std::vector<uint8_t> data = {0, 1, 1, 2, 3, 5, 8};
  EXPECT_CALL(*key, SignSlowly(ElementsAreArray(data)))
      .Times(4)
      .WillRepeatedly(Return(std::nullopt));
  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(key), UnexportableKeyId());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  task_manager().SignSlowlyAsync(GetParam().origin, ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         ServiceError::kCryptoApiFailed);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(3, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       RetrySignAsyncIfSignatureVerificationFailsWithSuccess) {
  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
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
  task_manager().SignSlowlyAsync(GetParam().origin, ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_OK(sign_future.Get());
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(1, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
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
  task_manager().SignSlowlyAsync(GetParam().origin, ref_counted_key, data,
                                 BackgroundTaskPriority::kBestEffort,
                                 sign_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kVerifySignatureFailed));
  VerifyResultHistograms(histogram_tester, kSignTaskType,
                         ServiceError::kVerifySignatureFailed);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kSignTaskType)),
              ElementsAre(base::Bucket(3, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, DeleteKeysAsync) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // First, generate two new signing keys.
  scoped_provider.AddNextGeneratedKey(std::make_unique<MockUnexportableKey>());
  scoped_provider.AddNextGeneratedKey(std::make_unique<MockUnexportableKey>());

  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key1,
                       generate_key_future.Get());

  generate_key_future.Clear();
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key2,
                       generate_key_future.Get());

  // Second, delete the keys.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_keys_future;
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAre(&key1->key(), &key2->key())))
      .WillOnce(Return(2));

  task_manager().DeleteSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      {key1, key2}, BackgroundTaskPriority::kBestEffort,
      delete_keys_future.GetCallback());
  EXPECT_FALSE(delete_keys_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_keys_future.IsReady());
  EXPECT_THAT(delete_keys_future.Get(), ValueIs(2u));
  VerifyResultHistograms(histogram_tester, kDeleteKeysTaskType,
                         kNoServiceErrorForMetrics);

  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kDeleteKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, DeleteKeysAsyncPartialSuccess) {
  ScopedMockUnexportableKeyProvider& scoped_provider =
      SwitchToMockKeyProvider();

  // First, generate two new signing keys.
  scoped_provider.AddNextGeneratedKey(std::make_unique<MockUnexportableKey>());
  scoped_provider.AddNextGeneratedKey(std::make_unique<MockUnexportableKey>());

  base::test::TestFuture<
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      generate_key_future;
  auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key1,
                       generate_key_future.Get());

  generate_key_future.Clear();
  task_manager().GenerateSigningKeySlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      supported_algorithm, BackgroundTaskPriority::kBestEffort,
      generate_key_future.GetCallback());
  RunBackgroundTasks();
  ASSERT_OK_AND_ASSIGN(scoped_refptr<RefCountedUnexportableSigningKey> key2,
                       generate_key_future.Get());

  // Second, delete the keys.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_keys_future;
  // Simulate a partial success.
  EXPECT_CALL(scoped_provider.mock(),
              DeleteSigningKeysSlowly(ElementsAre(&key1->key(), &key2->key())))
      .WillOnce(Return(1));

  task_manager().DeleteSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      {key1, key2}, BackgroundTaskPriority::kBestEffort,
      delete_keys_future.GetCallback());
  EXPECT_FALSE(delete_keys_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_keys_future.IsReady());
  EXPECT_THAT(delete_keys_future.Get(), ValueIs(1u));
  VerifyResultHistograms(histogram_tester, kDeleteKeysTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kDeleteKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, DeleteKeysAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_keys_future;

  DisableKeyProvider();
  task_manager().DeleteSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      {MakeRefCountedKey({1, 2, 3})}, BackgroundTaskPriority::kBestEffort,
      delete_keys_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_keys_future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  VerifyResultHistograms(histogram_tester, kDeleteKeysTaskType,
                         ServiceError::kNoKeyProvider);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kDeleteKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       DeleteKeysAsyncFailureOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_keys_future;

  task_manager().DeleteSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      {MakeRefCountedKey({1, 2, 3})}, BackgroundTaskPriority::kBestEffort,
      delete_keys_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_keys_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
  VerifyResultHistograms(histogram_tester, kDeleteKeysTaskType,
                         ServiceError::kOperationNotSupported);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kDeleteKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, DeleteAllKeysAsync) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly())
      .WillOnce(Return(1u));

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  EXPECT_FALSE(delete_all_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_all_future.IsReady());
  EXPECT_THAT(delete_all_future.Get(), ValueIs(1u));
  VerifyResultHistograms(histogram_tester, kDeleteAllKeysTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesSuccessHistogramNameFormat, kDeleteAllKeysTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       DeleteAllKeysAsyncFailureCryptoApiFailed) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), DeleteAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  EXPECT_FALSE(delete_all_future.IsReady());
  RunBackgroundTasks();

  EXPECT_TRUE(delete_all_future.IsReady());
  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  VerifyResultHistograms(histogram_tester, kDeleteAllKeysTaskType,
                         ServiceError::kCryptoApiFailed);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kDeleteAllKeysTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest, DeleteAllKeysAsyncFailureNoKeyProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  DisableKeyProvider();
  task_manager().DeleteAllSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_all_future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  VerifyResultHistograms(histogram_tester, kDeleteAllKeysTaskType,
                         ServiceError::kNoKeyProvider);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kDeleteAllKeysTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       DeleteAllKeysAsyncFailureOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<size_t>> delete_all_future;

  task_manager().DeleteAllSigningKeysSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, delete_all_future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(delete_all_future.Get(),
              ErrorIs(ServiceError::kOperationNotSupported));
  VerifyResultHistograms(histogram_tester, kDeleteAllKeysTaskType,
                         ServiceError::kOperationNotSupported);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(absl::StrFormat(
          kTaskRetriesFailureHistogramNameFormat, kDeleteAllKeysTaskType)),
      ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncNoKeys) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(
          std::vector<std::unique_ptr<crypto::UnexportableSigningKey>>()));

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(auto keys, future.Get());
  EXPECT_TRUE(keys.empty());

  VerifyResultHistograms(histogram_tester, kGetAllKeysTaskType,
                         kNoServiceErrorForMetrics);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesSuccessHistogramNameFormat, kGetAllKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
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
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  ASSERT_OK_AND_ASSIGN(auto keys, future.Get());
  EXPECT_EQ(keys.size(), 1u);
}

TEST_P(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncProviderFails) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  EXPECT_CALL(SwitchToMockKeyProvider().mock(), GetAllSigningKeysSlowly())
      .WillOnce(Return(std::nullopt));

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
  VerifyResultHistograms(histogram_tester, kGetAllKeysTaskType,
                         ServiceError::kCryptoApiFailed);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kGetAllKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncNoProvider) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  DisableKeyProvider();

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kNoKeyProvider));
  VerifyResultHistograms(histogram_tester, kGetAllKeysTaskType,
                         ServiceError::kNoKeyProvider);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kGetAllKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

TEST_P(UnexportableKeyTaskManagerTest,
       GetAllSigningKeysForGarbageCollectionAsyncOperationNotSupported) {
  ASSERT_EQ(UnexportableKeyTaskManager::GetUnexportableKeyProvider({})
                ->AsStatefulUnexportableKeyProvider(),
            nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ServiceErrorOr<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>>
      future;

  task_manager().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      GetParam().origin, crypto::UnexportableKeyProvider::Config(),
      BackgroundTaskPriority::kBestEffort, future.GetCallback());
  RunBackgroundTasks();

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kOperationNotSupported));
  VerifyResultHistograms(histogram_tester, kGetAllKeysTaskType,
                         ServiceError::kOperationNotSupported);
  EXPECT_THAT(histogram_tester.GetAllSamples(absl::StrFormat(
                  kTaskRetriesFailureHistogramNameFormat, kGetAllKeysTaskType)),
              ElementsAre(base::Bucket(0, 1)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    UnexportableKeyTaskManagerTest,
    Values(
        TestParams{
            .origin = BackgroundTaskOrigin::kRefreshTokenBinding,
            .expected_origin_suffix = ".RefreshTokenBinding",
            .test_suffix = "RefreshTokenBinding",
        },
        TestParams{
            .origin = BackgroundTaskOrigin::kDeviceBoundSessionCredentials,
            .expected_origin_suffix = ".DeviceBoundSessions",
            .test_suffix = "DeviceBoundSessions",
        },
        TestParams{
            .origin =
                BackgroundTaskOrigin::kDeviceBoundSessionCredentialsPrototype,
            .expected_origin_suffix = ".BoundSessionCredentials",
            .test_suffix = "BoundSessionCredentials",
        },
        TestParams{
            .origin = BackgroundTaskOrigin::kOrphanedKeyGarbageCollection,
            .expected_origin_suffix = ".OrphanedKeyGarbageCollection",
            .test_suffix = "OrphanedKeyGarbageCollection",
        }),
    [](const testing::TestParamInfo<TestParams>& info) {
      return info.param.test_suffix;
    });

}  // namespace unexportable_keys
