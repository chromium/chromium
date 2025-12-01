// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mojom/unexportable_key_service_proxied.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAreArray;

namespace {

constexpr auto kTestSubjectPublicKeyInfo = std::to_array<uint8_t>({1, 2, 3, 4});
constexpr auto kTestWrappedKey = std::to_array<uint8_t>({5, 6, 7, 8});

class FakeUnexportableKeyServiceProxy : public mojom::UnexportableKeyService {
 public:
  FakeUnexportableKeyServiceProxy() = default;
  ~FakeUnexportableKeyServiceProxy() override = default;

  void GenerateSigningKey(
      const std::vector<crypto::SignatureVerifier::SignatureAlgorithm>&
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      GenerateSigningKeyCallback callback) override {
    if (generate_response_) {
      std::move(callback).Run(std::move(generate_response_.value()));
      generate_response_.reset();
    } else if (acceptable_algorithms.empty()) {
      std::move(callback).Run(
          base::unexpected(ServiceError::kAlgorithmNotSupported));
    } else {
      mojom::NewKeyDataPtr new_key_data = mojom::NewKeyData::New();
      new_key_data->key_id =
          UnexportableKeyId(base::UnguessableToken::Create());
      new_key_data->subject_public_key_info =
          base::ToVector(kTestSubjectPublicKeyInfo);
      new_key_data->wrapped_key = base::ToVector(kTestWrappedKey);
      new_key_data->algorithm = acceptable_algorithms[0];
      std::move(callback).Run(std::move(new_key_data));
    }
  }

  void FromWrappedSigningKey(const std::vector<uint8_t>& wrapped_key,
                             BackgroundTaskPriority priority,
                             FromWrappedSigningKeyCallback callback) override {
    if (from_wrapped_response_) {
      std::move(callback).Run(std::move(from_wrapped_response_.value()));
      from_wrapped_response_.reset();
    } else if (wrapped_key.empty()) {
      std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    } else {
      mojom::NewKeyDataPtr new_key_data = mojom::NewKeyData::New();
      new_key_data->key_id =
          UnexportableKeyId(base::UnguessableToken::Create());
      new_key_data->subject_public_key_info =
          base::ToVector(kTestSubjectPublicKeyInfo);
      new_key_data->wrapped_key = wrapped_key;
      new_key_data->algorithm =
          crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
      std::move(callback).Run(std::move(new_key_data));
    }
  }

  void Sign(const UnexportableKeyId& key_id,
            const std::vector<uint8_t>& data,
            BackgroundTaskPriority priority,
            SignCallback callback) override {
    if (sign_response_) {
      std::move(callback).Run(std::move(sign_response_.value()));
      sign_response_.reset();
    } else if (data.empty()) {
      std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    } else {
      std::vector<uint8_t> signature = {0x11, 0x22, 0x33, 0x44};
      std::move(callback).Run(std::move(signature));
    }
  }

  void GetAllSigningKeysForGarbageCollection(
      BackgroundTaskPriority priority,
      GetAllSigningKeysForGarbageCollectionCallback callback) override {
    if (get_all_keys_response_) {
      std::move(callback).Run(std::move(get_all_keys_response_.value()));
      get_all_keys_response_.reset();
    } else {
      std::move(callback).Run(base::ok(std::vector<UnexportableKeyId>()));
    }
  }

  void DeleteKey(const UnexportableKeyId& key_id,
                 BackgroundTaskPriority priority,
                 DeleteKeyCallback callback) override {
    if (delete_key_response_) {
      std::move(callback).Run(std::move(delete_key_response_.value()));
      delete_key_response_.reset();
    } else {
      std::move(callback).Run(std::nullopt);
    }
  }

  void DeleteAllKeys(BackgroundTaskPriority priority,
                     DeleteAllKeysCallback callback) override {
    std::move(callback).Run(std::move(delete_all_keys_response_.value()));
    delete_all_keys_response_.reset();
  }

  void SetGenerateResponse(
      base::expected<mojom::NewKeyDataPtr, ServiceError> response) {
    generate_response_ = std::move(response);
  }

  void SetFromWrappedResponse(
      base::expected<mojom::NewKeyDataPtr, ServiceError> response) {
    from_wrapped_response_ = std::move(response);
  }

  void SetSignResponse(
      base::expected<std::vector<uint8_t>, ServiceError> response) {
    sign_response_ = std::move(response);
  }

  void SetGetAllSigningKeysForGarbageCollectionResponse(
      base::expected<std::vector<UnexportableKeyId>, ServiceError> response) {
    get_all_keys_response_ = std::move(response);
  }

  void SetDeleteKeyResponse(std::optional<ServiceError> response) {
    delete_key_response_ = std::move(response);
  }

  void SetDeleteAllKeysResponse(
      base::expected<uint64_t, ServiceError> response) {
    delete_all_keys_response_ = std::move(response);
  }

 private:
  std::optional<base::expected<mojom::NewKeyDataPtr, ServiceError>>
      generate_response_;
  std::optional<base::expected<mojom::NewKeyDataPtr, ServiceError>>
      from_wrapped_response_;
  std::optional<base::expected<std::vector<uint8_t>, ServiceError>>
      sign_response_;
  std::optional<base::expected<std::vector<UnexportableKeyId>, ServiceError>>
      get_all_keys_response_;
  std::optional<std::optional<ServiceError>> delete_key_response_;
  std::optional<base::expected<uint64_t, ServiceError>>
      delete_all_keys_response_;
};

class UnexportableKeyServiceProxiedTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  FakeUnexportableKeyServiceProxy fake_service_;
  mojo::Receiver<mojom::UnexportableKeyService> receiver_{&fake_service_};
  UnexportableKeyServiceProxied proxied_service_{
      receiver_.BindNewPipeAndPassRemote()};
};

TEST_F(UnexportableKeyServiceProxiedTest, GenerateSigningKeySuccess) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};

  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible, future.GetCallback());

  const ServiceErrorOr<UnexportableKeyId>& result = future.Get();
  ASSERT_TRUE(result.has_value());
  UnexportableKeyId key_id = result.value();

  EXPECT_THAT(proxied_service_.GetSubjectPublicKeyInfo(key_id),
              ValueIs(ElementsAreArray(kTestSubjectPublicKeyInfo)));
  EXPECT_THAT(proxied_service_.GetWrappedKey(key_id),
              ValueIs(ElementsAreArray(kTestWrappedKey)));
  EXPECT_THAT(
      proxied_service_.GetAlgorithm(key_id),
      ValueIs(crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256));
}

TEST_F(UnexportableKeyServiceProxiedTest, GenerateSigningKeyError) {
  fake_service_.SetGenerateResponse(
      base::unexpected(ServiceError::kCryptoApiFailed));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};

  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kCryptoApiFailed));
}

TEST_F(UnexportableKeyServiceProxiedTest, GenerateSigningKeyEmptyAlgorithms) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {};

  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kAlgorithmNotSupported));
}

TEST_F(UnexportableKeyServiceProxiedTest, GenerateKeyCollision) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future1;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible, future1.GetCallback());
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future1.Get().has_value());
  UnexportableKeyId key_id = future1.Get().value();

  mojom::NewKeyDataPtr collision_data = mojom::NewKeyData::New();
  collision_data->key_id = UnexportableKeyId(base::UnguessableToken(key_id));
  collision_data->subject_public_key_info = {9, 9};
  collision_data->wrapped_key = {9, 9, 9};
  collision_data->algorithm =
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  fake_service_.SetGenerateResponse(std::move(collision_data));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future2;
  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible, future2.GetCallback());
  ASSERT_TRUE(future2.Wait());
  EXPECT_THAT(future2.Get(), ErrorIs(ServiceError::kKeyCollision));
}

TEST_F(UnexportableKeyServiceProxiedTest, FromWrappedSigningKeySuccess) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  std::vector<uint8_t> wrapped_key = {0x11, 0x22, 0x33};

  proxied_service_.FromWrappedSigningKeySlowlyAsync(
      wrapped_key, BackgroundTaskPriority::kUserVisible, future.GetCallback());

  const ServiceErrorOr<UnexportableKeyId>& result = future.Get();
  ASSERT_TRUE(result.has_value());
  UnexportableKeyId key_id = result.value();

  EXPECT_THAT(proxied_service_.GetSubjectPublicKeyInfo(key_id),
              ValueIs(ElementsAreArray(kTestSubjectPublicKeyInfo)));
  EXPECT_THAT(proxied_service_.GetWrappedKey(key_id),
              ValueIs(ElementsAreArray(wrapped_key)));
  EXPECT_THAT(
      proxied_service_.GetAlgorithm(key_id),
      ValueIs(crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256));
}

TEST_F(UnexportableKeyServiceProxiedTest, FromWrappedSigningKeyAlreadyCached) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible,
      generate_future.GetCallback());
  ASSERT_TRUE(generate_future.Get().has_value());
  UnexportableKeyId key_id = generate_future.Get().value();

  ServiceErrorOr<std::vector<uint8_t>> original_spki =
      proxied_service_.GetSubjectPublicKeyInfo(key_id);
  ServiceErrorOr<std::vector<uint8_t>> original_wrapped =
      proxied_service_.GetWrappedKey(key_id);
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> original_algo =
      proxied_service_.GetAlgorithm(key_id);
  ASSERT_TRUE(original_spki.has_value());
  ASSERT_TRUE(original_wrapped.has_value());
  ASSERT_TRUE(original_algo.has_value());

  mojom::NewKeyDataPtr new_key_data = mojom::NewKeyData::New();
  new_key_data->key_id = UnexportableKeyId(base::UnguessableToken(key_id));
  new_key_data->subject_public_key_info = {99, 99};
  new_key_data->wrapped_key = {99};
  new_key_data->algorithm =
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;

  fake_service_.SetFromWrappedResponse(std::move(new_key_data));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> from_wrapped_future;
  std::vector<uint8_t> wrapped_key = {0xaa, 0xbb};
  proxied_service_.FromWrappedSigningKeySlowlyAsync(
      wrapped_key, BackgroundTaskPriority::kUserVisible,
      from_wrapped_future.GetCallback());

  EXPECT_THAT(from_wrapped_future.Get(), ValueIs(key_id));

  EXPECT_THAT(proxied_service_.GetSubjectPublicKeyInfo(key_id),
              ValueIs(original_spki.value()));
  EXPECT_THAT(proxied_service_.GetWrappedKey(key_id),
              ValueIs(original_wrapped.value()));
  EXPECT_THAT(proxied_service_.GetAlgorithm(key_id),
              ValueIs(original_algo.value()));
}

TEST_F(UnexportableKeyServiceProxiedTest, FromWrappedSigningKeyError) {
  fake_service_.SetFromWrappedResponse(
      base::unexpected(ServiceError::kKeyNotFound));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  std::vector<uint8_t> wrapped_key = {0x11, 0x22, 0x33};
  proxied_service_.FromWrappedSigningKeySlowlyAsync(
      wrapped_key, BackgroundTaskPriority::kUserVisible, future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(ServiceError::kKeyNotFound));
}

TEST_F(UnexportableKeyServiceProxiedTest, SignSuccess) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible,
      generate_future.GetCallback());
  ASSERT_TRUE(generate_future.Get().has_value());
  UnexportableKeyId key_id = generate_future.Get().value();

  std::vector<uint8_t> expected_signature = {0xaa, 0xbb, 0xcc, 0xdd};
  fake_service_.SetSignResponse(expected_signature);

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data_to_sign = {1, 2, 3, 4, 5, 6};
  proxied_service_.SignSlowlyAsync(key_id, data_to_sign,
                                   BackgroundTaskPriority::kUserVisible,
                                   sign_future.GetCallback());

  EXPECT_THAT(sign_future.Get(), ValueIs(expected_signature));
}

TEST_F(UnexportableKeyServiceProxiedTest, SignError) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algos = {
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
  proxied_service_.GenerateSigningKeySlowlyAsync(
      algos, BackgroundTaskPriority::kUserVisible,
      generate_future.GetCallback());
  ASSERT_TRUE(generate_future.Get().has_value());
  UnexportableKeyId key_id = generate_future.Get().value();

  fake_service_.SetSignResponse(
      base::unexpected(ServiceError::kVerifySignatureFailed));

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  std::vector<uint8_t> data_to_sign = {1, 2, 3, 4, 5, 6};
  proxied_service_.SignSlowlyAsync(key_id, data_to_sign,
                                   BackgroundTaskPriority::kUserVisible,
                                   sign_future.GetCallback());

  EXPECT_THAT(sign_future.Get(), ErrorIs(ServiceError::kVerifySignatureFailed));
}

TEST_F(UnexportableKeyServiceProxiedTest, GettersKeyNotFound) {
  UnexportableKeyId unknown_key_id(base::UnguessableToken::Create());

  EXPECT_THAT(proxied_service_.GetSubjectPublicKeyInfo(unknown_key_id),
              ErrorIs(ServiceError::kKeyNotFound));
  EXPECT_THAT(proxied_service_.GetWrappedKey(unknown_key_id),
              ErrorIs(ServiceError::kKeyNotFound));
  EXPECT_THAT(proxied_service_.GetAlgorithm(unknown_key_id),
              ErrorIs(ServiceError::kKeyNotFound));
}

}  // namespace
}  // namespace unexportable_keys
