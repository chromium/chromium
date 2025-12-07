// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>

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
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
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
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

constexpr BackgroundTaskPriority kTestPriority =
    BackgroundTaskPriority::kBestEffort;

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

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1};

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableKeyId key_id;

  const crypto::SignatureVerifier::SignatureAlgorithm algo =
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  const std::vector<uint8_t> wrapped_key = {0x11, 0x22, 0x33, 0x44};
  const std::vector<uint8_t> pub_key_info = {0x55, 0x66, 0x77, 0x88, 0x99};

  EXPECT_CALL(
      mock_uks,
      GenerateSigningKeySlowlyAsync(
          ElementsAre(
              crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
              crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256),
          kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id)).WillOnce(Return(algo));
  EXPECT_CALL(mock_uks, GetWrappedKey(key_id)).WillOnce(Return(wrapped_key));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(pub_key_info));

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  ASSERT_TRUE(res.has_value());

  const mojom::NewKeyDataPtr& new_key_data = *res;
  EXPECT_THAT(new_key_data->key_id, Eq(key_id));
  EXPECT_THAT(new_key_data->algorithm, Eq(algo));
  EXPECT_THAT(new_key_data->wrapped_key, Eq(wrapped_key));
  EXPECT_THAT(new_key_data->subject_public_key_info, Eq(pub_key_info));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetAlgorithmError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  ON_CALL(mock_uks, GetWrappedKey(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0x11, 0x22}));
  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB}));

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());
  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetWrappedKeyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kKeyNotFound)));

  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB, 0xCC}));

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, GenerateKeyGetSubjectPublicKeyInfoError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  UnexportableKeyId key_id;

  EXPECT_CALL(mock_uks, GenerateSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(std::vector<uint8_t>{0x11, 0x22, 0x33}));

  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, kTestPriority, future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kCryptoApiFailed));
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

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken unguessable_token =
      base::UnguessableToken::Create();
  UnexportableKeyId key_id(unguessable_token);

  const std::vector<uint8_t> test_wrapped_key = {0xAA, 0xBB, 0xCC};
  const crypto::SignatureVerifier::SignatureAlgorithm algo =
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256;
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

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  ASSERT_TRUE(res.has_value());

  const mojom::NewKeyDataPtr& new_key_data = *res;
  EXPECT_THAT(new_key_data->key_id, Eq(key_id));
  EXPECT_THAT(new_key_data->algorithm, Eq(algo));
  EXPECT_THAT(new_key_data->wrapped_key, Eq(wrapped_key_result));
  EXPECT_THAT(new_key_data->subject_public_key_info, Eq(pub_key_info));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeyGetAlgorithmError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken unguessable_token =
      base::UnguessableToken::Create();
  UnexportableKeyId key_id(unguessable_token);
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  ON_CALL(mock_uks, GetWrappedKey(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0x11, 0x22}));
  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB}));

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST(UnexportableKeyServiceProxyTest, FromWrappedKeyGetWrappedKeyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken unguessable_token =
      base::UnguessableToken::Create();
  UnexportableKeyId key_id(unguessable_token);
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kKeyNotFound)));

  ON_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillByDefault(Return(std::vector<uint8_t>{0xAA, 0xBB, 0xCC}));

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kKeyNotFound));
}

TEST(UnexportableKeyServiceProxyTest,
     FromWrappedKeyGetSubjectPublicKeyInfoError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken unguessable_token =
      base::UnguessableToken::Create();
  UnexportableKeyId key_id(unguessable_token);
  const std::vector<uint8_t> test_wrapped_key = {0x01, 0x02};

  EXPECT_CALL(mock_uks, FromWrappedSigningKeySlowlyAsync)
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id))
      .WillOnce(
          Return(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));

  EXPECT_CALL(mock_uks, GetWrappedKey(key_id))
      .WillOnce(Return(std::vector<uint8_t>{0x11, 0x22, 0x33}));

  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kCryptoApiFailed)));

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->FromWrappedSigningKey(test_wrapped_key, kTestPriority,
                                    future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kCryptoApiFailed));
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

  base::test::TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>>
      future;
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
  const base::UnguessableToken test_token = base::UnguessableToken::Create();
  const UnexportableKeyId key_id(test_token);
  const std::vector<uint8_t> test_data = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> expected_signature = {0xAA, 0xBB, 0xCC, 0xDD};

  EXPECT_CALL(mock_uks,
              SignSlowlyAsync(Eq(key_id), Eq(test_data), kTestPriority, _))
      .WillOnce(RunOnceCallback<3>(base::ok(expected_signature)));

  TestFuture<base::expected<std::vector<uint8_t>, ServiceError>> future;
  uks_remote->Sign(key_id, test_data, kTestPriority, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, ValueIs(expected_signature));
}

TEST(UnexportableKeyServiceProxyTest, SignError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken test_token = base::UnguessableToken::Create();
  const UnexportableKeyId key_id(test_token);
  const std::vector<uint8_t> test_data = {0xFF, 0xEE};
  const ServiceError expected_error = ServiceError::kKeyNotFound;

  EXPECT_CALL(mock_uks,
              SignSlowlyAsync(Eq(key_id), Eq(test_data), kTestPriority, _))
      .WillOnce(RunOnceCallback<3>(base::unexpected(expected_error)));

  TestFuture<base::expected<std::vector<uint8_t>, ServiceError>> future;
  uks_remote->Sign(key_id, test_data, kTestPriority, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest,
     GetAllSigningKeysForGarbageCollectionSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const UnexportableKeyId key_id1;
  const UnexportableKeyId key_id2;
  std::vector<UnexportableKeyId> mock_result = {key_id1, key_id2};

  EXPECT_CALL(mock_uks, GetAllSigningKeysForGarbageCollectionSlowlyAsync(
                            kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::ok(mock_result)));

  TestFuture<base::expected<std::vector<UnexportableKeyId>, ServiceError>>
      future;
  uks_remote->GetAllSigningKeysForGarbageCollection(kTestPriority,
                                                    future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, ValueIs(UnorderedElementsAre(key_id1, key_id2)));
}

TEST(UnexportableKeyServiceProxyTest,
     GetAllSigningKeysForGarbageCollectionError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  ServiceError expected_error = ServiceError::kCryptoApiFailed;

  EXPECT_CALL(mock_uks, GetAllSigningKeysForGarbageCollectionSlowlyAsync(
                            kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::unexpected(expected_error)));

  TestFuture<base::expected<std::vector<UnexportableKeyId>, ServiceError>>
      future;
  uks_remote->GetAllSigningKeysForGarbageCollection(kTestPriority,
                                                    future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, ErrorIs(expected_error));
}

TEST(UnexportableKeyServiceProxyTest, DeleteKeySuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken test_token = base::UnguessableToken::Create();
  UnexportableKeyId key_id(test_token);

  EXPECT_CALL(mock_uks, DeleteKeySlowlyAsync(Eq(key_id), kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(base::ok()));

  TestFuture<std::optional<ServiceError>> future;
  uks_remote->DeleteKey(key_id, kTestPriority, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, Eq(std::nullopt));
}

TEST(UnexportableKeyServiceProxyTest, DeleteKeyError) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  const base::UnguessableToken test_token = base::UnguessableToken::Create();
  UnexportableKeyId key_id(test_token);
  ServiceError expected_error = ServiceError::kKeyNotFound;

  EXPECT_CALL(mock_uks, DeleteKeySlowlyAsync(Eq(key_id), kTestPriority, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(expected_error)));

  TestFuture<std::optional<ServiceError>> future;
  uks_remote->DeleteKey(key_id, kTestPriority, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_THAT(result, Optional(Eq(expected_error)));
}

TEST(UnexportableKeyServiceProxyTest, DeleteAllKeysSuccess) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::UnexportableKeyService> uks_remote;
  mojo::PendingReceiver<mojom::UnexportableKeyService> receiver =
      uks_remote.BindNewPipeAndPassReceiver();

  MockUnexportableKeyService mock_uks;
  UnexportableKeyServiceProxyImpl proxy_impl(&mock_uks, std::move(receiver));

  // Expect the call to the mock service's async method and simulate success.
  EXPECT_CALL(mock_uks, DeleteAllKeysSlowlyAsync(kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::ok(1)));

  TestFuture<base::expected<uint64_t, ServiceError>> future;
  uks_remote->DeleteAllKeys(kTestPriority, future.GetCallback());

  // The AdaptErrorOrVoid should convert base::ok() to std::nullopt.
  const auto& result = future.Get();
  EXPECT_THAT(result, ValueIs(1));
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
  EXPECT_CALL(mock_uks, DeleteAllKeysSlowlyAsync(kTestPriority, _))
      .WillOnce(RunOnceCallback<1>(base::unexpected(expected_error)));

  TestFuture<base::expected<uint64_t, ServiceError>> future;
  uks_remote->DeleteAllKeys(kTestPriority, future.GetCallback());

  // The AdaptErrorOrVoid should propagate the ServiceError.
  const auto& result = future.Get();
  EXPECT_THAT(result, ErrorIs(expected_error));
}
}  // namespace unexportable_keys
