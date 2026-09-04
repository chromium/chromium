// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

constexpr BackgroundTaskPriority kTestPriority =
    BackgroundTaskPriority::kBestEffort;

constexpr auto kTestWrappedKey =
    std::to_array<uint8_t>({0x11, 0x22, 0x33, 0x44});
constexpr auto kTestSubjectPublicKeyInfo =
    std::to_array<uint8_t>({0x55, 0x66, 0x77, 0x88, 0x99});
constexpr auto kTestChallenge = std::to_array<uint8_t>({0x01, 0x02, 0x03});
constexpr auto kTestWrappedAttestationKey =
    std::to_array<uint8_t>({0xAA, 0xBB, 0xCC});
constexpr auto kTestAttestationStatement = std::to_array<uint8_t>({0x11, 0x22});
constexpr auto kTestAttestationSignature = std::to_array<uint8_t>({0x33, 0x44});

constexpr std::array kTestAttestationAlgorithms = {
    crypto::sign::RSA_PKCS1_SHA1,
    crypto::sign::ECDSA_SHA256,
};
constexpr std::array kTestAttestationAlgorithmsError = {
    crypto::sign::RSA_PKCS1_SHA1,
};
constexpr auto kTestAlgorithmECDSA = crypto::sign::ECDSA_SHA256;
constexpr auto kTestAlgorithmRSAPSS = crypto::sign::RSA_PSS_SHA256;

TEST(UnexportableKeyServiceProxyTest, GenerateKeyReturnsError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks.BindNewPipeAndPassReceiver();

  auto mock = MockUnexportableKeyService();
  ON_CALL(mock, GenerateSigningKeySlowlyAsync)
      .WillByDefault(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));
  UnexportableKeyServiceProxyImpl impl(&mock, std::move(receiver));

  std::vector<crypto::sign::SignatureKind> algos = {
      crypto::sign::RSA_PKCS1_SHA1};

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableSigningKeyId key_id;

  const crypto::sign::SignatureKind algo = crypto::sign::ECDSA_SHA256;
  const std::vector<uint8_t> wrapped_key = {0x11, 0x22, 0x33, 0x44};
  const std::vector<uint8_t> pub_key_info = {0x55, 0x66, 0x77, 0x88, 0x99};

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync(
                            ElementsAre(crypto::sign::RSA_PKCS1_SHA1,
                                        crypto::sign::ECDSA_SHA256),
                            kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id)).WillOnce(Return(algo));
  EXPECT_CALL(mock_uks, GetWrappedKey(key_id)).WillOnce(Return(wrapped_key));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(pub_key_info));

  std::vector<crypto::sign::SignatureKind> algos = {
      crypto::sign::RSA_PKCS1_SHA1, crypto::sign::ECDSA_SHA256};

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(mojom::NewSigningKeyDataPtr new_key_data, future.Take());
  EXPECT_THAT(new_key_data->key_id, Eq(key_id));
  EXPECT_THAT(new_key_data->metadata->algorithm, Eq(algo));
  EXPECT_THAT(new_key_data->metadata->wrapped_key, Eq(wrapped_key));
  EXPECT_THAT(new_key_data->metadata->subject_public_key_info,
              Eq(pub_key_info));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetAlgorithmError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableSigningKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  ON_CALL(mock_uks, GetWrappedKey(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0x11, 0x22}));
  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB}));

  std::vector<crypto::sign::SignatureKind> algos = {
      crypto::sign::RSA_PKCS1_SHA256};

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetWrappedKeyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableSigningKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(crypto::sign::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kKeyNotFound)));

  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB, 0xCC}));

  std::vector<crypto::sign::SignatureKind> algos = {
      crypto::sign::RSA_PKCS1_SHA256};

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetSubjectPublicKeyInfoError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableSigningKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(crypto::sign::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(std::vector<uint8_t>{0x11, 0x22, 0x33}));

  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  std::vector<crypto::sign::SignatureKind> algos = {crypto::sign::ECDSA_SHA256};

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeyReturnsError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02, 0x03};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync(Eq(test_wrapped_key),
                                                         kTestPriority, _))
      .WillOnce(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;

  const std::vector<uint8_t> test_wrapped_key = {0xAA, 0xBB, 0xCC};
  const crypto::sign::SignatureKind algo = crypto::sign::RSA_PSS_SHA256;
  const std::vector<uint8_t> wrapped_key_result = {0x11, 0x22, 0x33, 0x44};
  const std::vector<uint8_t> pub_key_info = {0x55, 0x66, 0x77, 0x88, 0x99};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync(Eq(test_wrapped_key),
                                                         kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id)).WillOnce(Return(algo));
  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(wrapped_key_result));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(pub_key_info));

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  ASSERT_OK_AND_ASSIGN(mojom::NewSigningKeyDataPtr new_key_data, future.Take());
  EXPECT_THAT(new_key_data->key_id, Eq(key_id));
  EXPECT_THAT(new_key_data->metadata->algorithm, Eq(algo));
  EXPECT_THAT(new_key_data->metadata->wrapped_key, Eq(wrapped_key_result));
  EXPECT_THAT(new_key_data->metadata->subject_public_key_info,
              Eq(pub_key_info));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeyGetAlgorithmError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  ON_CALL(mock_uks, GetWrappedKey(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0x11, 0x22}));
  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB}));

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeyGetWrappedKeyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(crypto::sign::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kKeyNotFound)));

  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB, 0xCC}));

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest,
     FromWrappedKeyGetSubjectPublicKeyInfoError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(crypto::sign::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(std::vector<uint8_t>{0x11, 0x22, 0x33}));

  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, TooLongWrappedSigningKey) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  mojo::test::BadMessageObserver bad_message_observer;

  const std::vector<uint8_t> test_wrapped_key(kMaxWrappedKeySize + 1);

  base::test::TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(bad_message_observer.got_bad_message())
      << "Expected mojo::ReportBadMessage to be called for a too-long wrapped "
         "key.";
}

TEST(UnexportableKeyServiceProxyTest, SignSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));
  const UnexportableSigningKeyId key_id;
  const std::vector<uint8_t> test_data = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> expected_signature = {0xAA, 0xBB, 0xCC, 0xDD};

  EXPECT_CALL(mock_uks,
              SignSlowlyAsync(Eq(key_id), Eq(test_data), kTestPriority, _))
      .WillOnce(RunOnceCallback<3>(base::ok(expected_signature)));

  TestFuture<ServiceErrorOr<std::vector<uint8_t>>> future;
  uks_remote->Sign(key_id, test_data, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ValueIs(expected_signature));
}

TEST(UnexportableKeyServiceProxyTest, SignError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));
  const UnexportableSigningKeyId key_id;
  const std::vector<uint8_t> test_data = {0xFF, 0xEE};
  const ServiceError expected_error = ServiceError::kKeyNotFound;

  EXPECT_CALL(mock_uks,
              SignSlowlyAsync(Eq(key_id), Eq(test_data), kTestPriority, _))
      .WillOnce(RunOnceCallback<3>(base::unexpected(expected_error)));

  TestFuture<ServiceErrorOr<std::vector<uint8_t>>> future;
  uks_remote->Sign(key_id, test_data, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest, GetAllKeysForGarbageCollectionSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableSigningKeyId key_id1;
  const UnexportableSigningKeyId key_id2;
  std::vector<UnexportableSigningKeyId> mock_result = {key_id1, key_id2};

  EXPECT_CALL(mock_uks,
              GetAllKeysForGarbageCollectionSlowlyAsync(kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::ok(mock_result)));

  // Proxy implementation calls accessors for each key.
  // We use ON_CALL to provide default success responses for these accessors.
  // Since PopulateNewKeyData calls them in order, we should ensure they return
  // valid data.
  ON_CALL(mock_uks, GetAlgorithm)
      .WillByDefault(Return(crypto::sign::ECDSA_SHA256));
  ON_CALL(mock_uks, GetWrappedKey)
      .WillByDefault(Return(std::vector<uint8_t>{1, 2, 3}));
  ON_CALL(mock_uks, GetSubjectPublicKeyInfo)
      .WillByDefault(Return(std::vector<uint8_t>{4, 5, 6}));
  ON_CALL(mock_uks, GetKeyTag).WillByDefault(Return("tag"));
  ON_CALL(mock_uks, GetCreationTime).WillByDefault(Return(base::Time::Now()));

  TestFuture<ServiceErrorOr<std::vector<mojom::NewSigningKeyDataPtr>>> future;
  uks_remote->GetAllKeysForGarbageCollection(kTestPriority,
                                             future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::vector<mojom::NewSigningKeyDataPtr> keys,
                       future.Take());
  ASSERT_THAT(keys, SizeIs(2));
  EXPECT_EQ(keys[0]->key_id, key_id1);
  EXPECT_EQ(keys[1]->key_id, key_id2);
}

TEST(UnexportableKeyServiceProxyTest, GetAllKeysForGarbageCollectionError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  ServiceError expected_error = ServiceError::kCryptoApiFailed;

  EXPECT_CALL(mock_uks,
              GetAllKeysForGarbageCollectionSlowlyAsync(kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::unexpected(expected_error)));

  TestFuture<ServiceErrorOr<std::vector<mojom::NewSigningKeyDataPtr>>> future;
  uks_remote->GetAllKeysForGarbageCollection(kTestPriority,
                                             future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest, DeleteKeysSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;

  EXPECT_CALL(mock_uks,
              DeleteKeysSlowlyAsync(ElementsAre(key_id), kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(1));

  TestFuture<ServiceErrorOr<uint64_t>> future;
  uks_remote->DeleteKeys({key_id}, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ValueIs(1));
}

TEST(UnexportableKeyServiceProxyTest, DeleteKeysError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableSigningKeyId key_id;
  ServiceError expected_error = ServiceError::kKeyNotFound;

  EXPECT_CALL(mock_uks,
              DeleteKeysSlowlyAsync(ElementsAre(key_id), kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(expected_error)));

  TestFuture<ServiceErrorOr<uint64_t>> future;
  uks_remote->DeleteKeys({key_id}, kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest, DeleteAllKeysSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  // Expect the call to the mock service's async method and simulate success.
  EXPECT_CALL(mock_uks, DeleteAllKeysSlowlyAsync)
      .WillOnce(RunOnceCallback<0>(base::ok(1)));

  TestFuture<ServiceErrorOr<uint64_t>> future;
  uks_remote->DeleteAllKeys(future.GetCallback());

  // The AdaptErrorOrVoid should convert base::ok() to std::nullopt.
  EXPECT_THAT(future.Get(), ValueIs(1));
}

TEST(UnexportableKeyServiceProxyTest, DeleteAllKeysError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  ServiceError expected_error = ServiceError::kCryptoApiFailed;

  // Expect the call to the mock service's async method and simulate an error.
  EXPECT_CALL(mock_uks, DeleteAllKeysSlowlyAsync)
      .WillOnce(RunOnceCallback<0>(base::unexpected(expected_error)));

  TestFuture<ServiceErrorOr<uint64_t>> future;
  uks_remote->DeleteAllKeys(future.GetCallback());

  // The AdaptErrorOrVoid should propagate the ServiceError.
  EXPECT_THAT(future.Get(), ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest, GenerateAttestationKeyReturnsError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks.BindNewPipeAndPassReceiver();

  auto mock = MockUnexportableKeyService();
  ON_CALL(mock, GenerateAttestationKeySlowlyAsync)
      .WillByDefault(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));
  UnexportableKeyServiceProxyImpl impl(&mock, std::move(receiver));

  TestFuture<ServiceErrorOr<mojom::NewAttestationKeyDataPtr>> future;
  uks->GenerateAttestationKey(base::ToVector(kTestAttestationAlgorithmsError),
                              kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, GenerateAttestationKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableAttestationKeyId kKeyId;

  EXPECT_CALL(mock_uks, GenerateAttestationKeySlowlyAsync(
                            ElementsAreArray(kTestAttestationAlgorithms),
                            kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(kKeyId));

  EXPECT_CALL(mock_uks, GetAlgorithm(kKeyId))
      .WillOnce(Return(kTestAlgorithmECDSA));
  EXPECT_CALL(mock_uks, GetWrappedKey(kKeyId))
      .WillOnce(Return(base::ToVector(kTestWrappedKey)));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(kKeyId))
      .WillOnce(Return(base::ToVector(kTestSubjectPublicKeyInfo)));

  TestFuture<ServiceErrorOr<mojom::NewAttestationKeyDataPtr>> future;
  uks_remote->GenerateAttestationKey(base::ToVector(kTestAttestationAlgorithms),
                                     kTestPriority, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(mojom::NewAttestationKeyDataPtr new_key_data,
                       future.Take());
  EXPECT_EQ(new_key_data->key_id, kKeyId);
  EXPECT_EQ(new_key_data->metadata->algorithm, kTestAlgorithmECDSA);
  EXPECT_EQ(new_key_data->metadata->wrapped_key,
            base::ToVector(kTestWrappedKey));
  EXPECT_EQ(new_key_data->metadata->subject_public_key_info,
            base::ToVector(kTestSubjectPublicKeyInfo));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedAttestationKeyReturnsError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  EXPECT_CALL(mock_uks, FromWrappedAttestationKeySlowlyAsync(Eq(kTestChallenge),
                                                             kTestPriority, _))
      .WillOnce(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));

  TestFuture<ServiceErrorOr<mojom::NewAttestationKeyDataPtr>> future;
  uks_remote->FromWrappedAttestationKey(base::ToVector(kTestChallenge),
                                        kTestPriority, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedAttestationKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableAttestationKeyId kKeyId;

  EXPECT_CALL(mock_uks, FromWrappedAttestationKeySlowlyAsync(
                            Eq(kTestWrappedAttestationKey), kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(kKeyId));

  EXPECT_CALL(mock_uks, GetAlgorithm(kKeyId))
      .WillOnce(Return(kTestAlgorithmRSAPSS));
  EXPECT_CALL(mock_uks, GetWrappedKey(kKeyId))
      .WillOnce(Return(base::ToVector(kTestWrappedKey)));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(kKeyId))
      .WillOnce(Return(base::ToVector(kTestSubjectPublicKeyInfo)));

  TestFuture<ServiceErrorOr<mojom::NewAttestationKeyDataPtr>> future;
  uks_remote->FromWrappedAttestationKey(
      base::ToVector(kTestWrappedAttestationKey), kTestPriority,
      future.GetCallback());

  ASSERT_OK_AND_ASSIGN(mojom::NewAttestationKeyDataPtr new_key_data,
                       future.Take());
  EXPECT_EQ(new_key_data->key_id, kKeyId);
  EXPECT_EQ(new_key_data->metadata->algorithm, kTestAlgorithmRSAPSS);
  EXPECT_EQ(new_key_data->metadata->wrapped_key,
            base::ToVector(kTestWrappedKey));
  EXPECT_EQ(new_key_data->metadata->subject_public_key_info,
            base::ToVector(kTestSubjectPublicKeyInfo));
}

TEST(UnexportableKeyServiceProxyTest, TooLongWrappedAttestationKey) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  mojo::test::BadMessageObserver bad_message_observer;

  base::test::TestFuture<ServiceErrorOr<mojom::NewAttestationKeyDataPtr>>
      future;
  uks_remote->FromWrappedAttestationKey(
      std::vector<uint8_t>(kMaxWrappedKeySize + 1), kTestPriority,
      future.GetCallback());

  bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(bad_message_observer.got_bad_message())
      << "Expected mojo::ReportBadMessage to be called for a too-long wrapped "
         "key.";
}

TEST(UnexportableKeyServiceProxyTest, CertifySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));
  const UnexportableAttestationKeyId kAttestationKeyId;
  const UnexportableSigningKeyId kSigningKeyId;
  const crypto::AttestationStatement kExpectedStatement{
      .format = crypto::AttestationStatement::Format::kTpm,
      .statement = base::ToVector(kTestAttestationStatement),
      .signature = base::ToVector(kTestAttestationSignature),
  };

  EXPECT_CALL(mock_uks,
              CertifySlowlyAsync(kAttestationKeyId, kSigningKeyId,
                                 Eq(kTestChallenge), kTestPriority, _))
      .WillOnce(RunOnceCallback<4>(base::ok(kExpectedStatement)));

  TestFuture<ServiceErrorOr<crypto::AttestationStatement>> future;
  uks_remote->Certify(kAttestationKeyId, kSigningKeyId,
                      base::ToVector(kTestChallenge), kTestPriority,
                      future.GetCallback());

  ASSERT_OK_AND_ASSIGN(const crypto::AttestationStatement& statement,
                       future.Get());
  EXPECT_EQ(statement.format, kExpectedStatement.format);
  EXPECT_EQ(statement.statement, kExpectedStatement.statement);
  EXPECT_EQ(statement.signature, kExpectedStatement.signature);
}

TEST(UnexportableKeyServiceProxyTest, CertifyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));
  const UnexportableAttestationKeyId kAttestationKeyId;
  const UnexportableSigningKeyId kSigningKeyId;
  const ServiceError kExpectedError = ServiceError::kCryptoApiFailed;

  EXPECT_CALL(mock_uks,
              CertifySlowlyAsync(kAttestationKeyId, kSigningKeyId,
                                 Eq(kTestChallenge), kTestPriority, _))
      .WillOnce(RunOnceCallback<4>(base::unexpected(kExpectedError)));

  TestFuture<ServiceErrorOr<crypto::AttestationStatement>> future;
  uks_remote->Certify(kAttestationKeyId, kSigningKeyId,
                      base::ToVector(kTestChallenge), kTestPriority,
                      future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(kExpectedError));
}

TEST(UnexportableKeyServiceProxyTest, DestroyProxyBeforeCallback) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  MockUnexportableKeyService mock_uks;
  auto proxy = std::make_optional<UnexportableKeyServiceProxyImpl>(
      &mock_uks, uks_remote.BindNewPipeAndPassReceiver());

  // Use a unique ServiceError to distinguish the dropped callback case from all
  // others.
  constexpr auto kDroppedCallbackError = ServiceError::kKeyCollision;
  base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
      pending_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(WithArg<2>([&](auto cb) {
        pending_callback = std::move(cb);
        run_loop.Quit();
      }));
  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(
      {crypto::sign::RSA_PKCS1_SHA1}, kTestPriority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          future.GetCallback(), base::unexpected(kDroppedCallbackError)));

  run_loop.Run();
  ASSERT_TRUE(pending_callback);

  // Simulate the proxy being destroyed before the task completes.
  proxy.reset();

  // The callback is invoked after a background task completes.
  std::move(pending_callback)
      .Run(base::unexpected(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(future.Get(), ErrorIs(kDroppedCallbackError));
}

TEST(UnexportableKeyServiceProxyTest, DestroyProxyAndServiceBeforeCallback) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  auto mock_uks = std::make_optional<MockUnexportableKeyService>();
  auto proxy = std::make_optional<UnexportableKeyServiceProxyImpl>(
      &*mock_uks, uks_remote.BindNewPipeAndPassReceiver());

  // Use a unique ServiceError to distinguish the dropped callback case from all
  // others.
  constexpr auto kDroppedCallbackError = ServiceError::kKeyCollision;
  base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
      pending_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(WithArg<2>([&](auto cb) {
        pending_callback = std::move(cb);
        run_loop.Quit();
      }));
  TestFuture<ServiceErrorOr<mojom::NewSigningKeyDataPtr>> future;
  uks_remote->GenerateSigningKey(
      {crypto::sign::RSA_PKCS1_SHA1}, kTestPriority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          future.GetCallback(), base::unexpected(kDroppedCallbackError)));

  run_loop.Run();
  ASSERT_TRUE(pending_callback);

  // Simulate both the proxy and the service being destroyed before the task
  // completes.
  proxy.reset();
  mock_uks.reset();

  // The callback is invoked after a background task completes.
  std::move(pending_callback)
      .Run(base::unexpected(ServiceError::kCryptoApiFailed));
  EXPECT_THAT(future.Get(), ErrorIs(kDroppedCallbackError));
}

}  // namespace unexportable_keys
