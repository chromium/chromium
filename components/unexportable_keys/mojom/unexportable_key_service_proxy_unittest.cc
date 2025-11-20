// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Return;

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
  uks->GenerateSigningKey(algos, BackgroundTaskPriority::kBestEffort,
                          future.GetCallback());

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
          BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(RunOnceCallback<2>(key_id));

  EXPECT_CALL(mock_uks, GetAlgorithm(key_id)).WillOnce(Return(algo));
  EXPECT_CALL(mock_uks, GetWrappedKey(key_id)).WillOnce(Return(wrapped_key));
  EXPECT_CALL(mock_uks, GetSubjectPublicKeyInfo(key_id))
      .WillOnce(Return(pub_key_info));

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};
  BackgroundTaskPriority priority = BackgroundTaskPriority::kUserVisible;

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, priority, future.GetCallback());

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
  BackgroundTaskPriority priority = BackgroundTaskPriority::kBestEffort;

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, priority, future.GetCallback());
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
  BackgroundTaskPriority priority = BackgroundTaskPriority::kBestEffort;

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, priority, future.GetCallback());

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
  BackgroundTaskPriority priority = BackgroundTaskPriority::kBestEffort;

  TestFuture<base::expected<mojom::NewKeyDataPtr, ServiceError>> future;
  uks_remote->GenerateSigningKey(algos, priority, future.GetCallback());

  const base::expected<mojom::NewKeyDataPtr, ServiceError>& res = future.Get();
  EXPECT_THAT(res, ErrorIs(ServiceError::kCryptoApiFailed));
}
}  // namespace unexportable_keys
