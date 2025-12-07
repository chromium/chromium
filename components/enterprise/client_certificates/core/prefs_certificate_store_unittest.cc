// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/prefs_certificate_store.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/mock_private_key.h"
#include "components/enterprise/client_certificates/core/mock_private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

constexpr char kTestIdentityName[] = "identity_name";
constexpr char kOtherTestIdentityName[] = "other_identity_name";
constexpr char kFakeWrappedValue[] = "some_wrapped_value";
constexpr char kInvalidKey[] = "invalid_key";

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

base::Value::Dict CreateFakeDictKey() {
  base::Value::Dict key_dict;
  key_dict.Set(kKey, base::Base64Encode(kFakeWrappedValue));
  key_dict.Set(kKeySource,
               static_cast<int>(PrivateKeySource::kUnexportableKey));
  return key_dict;
}

std::string GetEncodedCertificate(net::X509Certificate& certificate) {
  std::string pem_encoded_certificate;
  net::X509Certificate::GetPEMEncoded(certificate.cert_buffer(),
                                      &pem_encoded_certificate);
  return pem_encoded_certificate;
}

base::Value::Dict CreateValidIdentity() {
  base::Value::Dict identity;
  identity.Set(kKeyDetails, CreateFakeDictKey());
  return identity;
}

base::Value::Dict CreateValidIdentity(net::X509Certificate& certificate) {
  base::Value::Dict identity = CreateValidIdentity();
  identity.Set(kCertificate, GetEncodedCertificate(certificate));
  return identity;
}

}  // namespace

using base::test::ErrorIs;
using base::test::RunOnceCallback;
using base::test::ValueIs;
using testing::_;
using testing::Return;
using testing::StrictMock;

class PrefsCertificateStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(kTestIdentityName);
    prefs_.registry()->RegisterDictionaryPref(kOtherTestIdentityName);

    auto mock_key_factory =
        std::make_unique<StrictMock<MockPrivateKeyFactory>>();
    mock_key_factory_ = mock_key_factory.get();

    store_ = std::make_unique<PrefsCertificateStore>(
        &prefs_, std::move(mock_key_factory));
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrefsCertificateStore> store_;
  raw_ptr<MockPrivateKeyFactory> mock_key_factory_;
};

// Tests the success path of the CreatePrivateKey function:
// - Parameters are valid,
// - There is no preexisting valid identity with key details,
// - Private key creation succeeds,
// - Private key serialization succeeds.
TEST_F(PrefsCertificateStoreTest, CreatePrivateKey_Success) {
  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mocked_private_key, ToDict())
      .WillOnce(Return(CreateFakeDictKey()));

  EXPECT_CALL(*mock_key_factory_, CreatePrivateKey(_))
      .WillOnce(RunOnceCallback<0>(mocked_private_key));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  EXPECT_THAT(test_future.Get(), ValueIs(mocked_private_key));

  auto* key_details = prefs_.GetDict(kTestIdentityName).FindDict(kKeyDetails);
  EXPECT_GT(key_details->size(), 0U);
  EXPECT_EQ(*key_details->FindString(kKey),
            base::Base64Encode(kFakeWrappedValue));
  EXPECT_EQ(*key_details->FindInt(kKeySource),
            static_cast<int>(PrivateKeySource::kUnexportableKey));
}

// Tests that no key is returned and the prefs are not modified when
// attempting to create a private key using an identity name that is already
// used.
TEST_F(PrefsCertificateStoreTest, CreatePrivateKey_ConflictFailure) {
  base::Value::Dict key_dict;
  key_dict.Set(kKey, kInvalidKey);
  base::Value::Dict identity;
  identity.Set(kKeyDetails, std::move(key_dict));
  prefs_.SetDict(kTestIdentityName, std::move(identity));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kConflictingIdentity));
  EXPECT_EQ(*prefs_.GetDict(kTestIdentityName)
                 .FindDict(kKeyDetails)
                 ->FindString(kKey),
            kInvalidKey);
}

// Tests that no key is returned and the prefs is not modified when
// the create key call fails.
TEST_F(PrefsCertificateStoreTest, CreatePrivateKey_CreateKeyFailure) {
  EXPECT_CALL(*mock_key_factory_, CreatePrivateKey(_))
      .WillOnce(RunOnceCallback<0>(nullptr));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kCreateKeyFailed));
  EXPECT_EQ(prefs_.GetDict(kTestIdentityName).size(), 0U);
}

// Tests that a certificate can be saved to the prefs when a private key
// already exists.
TEST_F(PrefsCertificateStoreTest, CommitCertificate_SuccessWithPrivateKey) {
  auto test_cert = LoadTestCert();
  prefs_.SetDict(kTestIdentityName, CreateValidIdentity());

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);

  EXPECT_EQ(*prefs_.GetDict(kTestIdentityName).FindString(kCertificate),
            GetEncodedCertificate(*test_cert));
}

// Tests that a certificate can be saved to the prefs even when a private key
// does not already exist.
TEST_F(PrefsCertificateStoreTest, CommitCertificate_SuccessWithoutPrivateKey) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);

  EXPECT_EQ(*prefs_.GetDict(kTestIdentityName).FindString(kCertificate),
            GetEncodedCertificate(*test_cert));
}

// Tests that a certificate won't be saved to the prefs when the given
// certificate instance is invalid (nullptr).
TEST_F(PrefsCertificateStoreTest, CommitCertificate_InvalidCertificateFailure) {
  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, /*certificate=*/nullptr,
                            test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidCertificateInput);
  EXPECT_EQ(prefs_.GetDict(kTestIdentityName).size(), 0U);
}

// Tests that an existing private key is moved and a certificate can be saved
// to the prefs.
TEST_F(PrefsCertificateStoreTest, CommitIdentity_SuccessWithPrivateKey) {
  auto test_cert = LoadTestCert();
  prefs_.SetDict(kOtherTestIdentityName, CreateValidIdentity());

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);

  EXPECT_EQ(*prefs_.GetDict(kTestIdentityName).FindString(kCertificate),
            GetEncodedCertificate(*test_cert));
  EXPECT_EQ(prefs_.GetDict(kOtherTestIdentityName).size(), 0U);
}

// Tests that a certificate cannot be saved to the prefs when the temporary
// identity does not already exist i.e contains details.
TEST_F(PrefsCertificateStoreTest, CommitIdentity_FailWithoutIdentity) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), StoreError::kIdentityNotFound);

  EXPECT_EQ(prefs_.GetDict(kTestIdentityName).size(), 0U);
  EXPECT_EQ(prefs_.GetDict(kOtherTestIdentityName).size(), 0U);
}

// Tests that a certificate won't be saved to the prefs when the given
// certificate instance is invalid (nullptr).
TEST_F(PrefsCertificateStoreTest, CommitIdentity_InvalidCertificateFailure) {
  prefs_.SetDict(kOtherTestIdentityName, CreateValidIdentity());

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName,
                         /*certificate=*/nullptr, test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidCertificateInput);

  EXPECT_EQ(prefs_.GetDict(kTestIdentityName).size(), 0U);
}

// Tests that an identity stored in the prefs with a private key and
// certificate can be properly loaded into memory and returned.
TEST_F(PrefsCertificateStoreTest, GetIdentity_FullIdentitySuccess) {
  auto test_cert = LoadTestCert();
  prefs_.SetDict(kTestIdentityName, CreateValidIdentity(*test_cert));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mock_key_factory_, LoadPrivateKeyFromDict(_, _))
      .WillOnce((RunOnceCallback<1>(mocked_private_key)));

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_EQ(identity->private_key, mocked_private_key);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(identity->certificate.get()));
}

// Tests that an identity stored in the prefs with only a private key can be
// properly loaded into memory and returned.
TEST_F(PrefsCertificateStoreTest, GetIdentity_OnlyPrivateKeySuccess) {
  prefs_.SetDict(kTestIdentityName, CreateValidIdentity());

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mock_key_factory_, LoadPrivateKeyFromDict(_, _))
      .WillOnce((RunOnceCallback<1>(mocked_private_key)));

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_EQ(identity->private_key, mocked_private_key);
  EXPECT_FALSE(identity->certificate);
}

// Tests that an identity stored in the prefs with only a certificate can be
// properly loaded into memory and returned.
TEST_F(PrefsCertificateStoreTest, GetIdentity_OnlyCertificateSuccess) {
  auto test_cert = LoadTestCert();
  base::Value::Dict identity_dict;
  identity_dict.Set(kCertificate, GetEncodedCertificate(*test_cert));
  prefs_.SetDict(kTestIdentityName, std::move(identity_dict));

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_FALSE(identity->private_key);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(identity->certificate.get()));
}

// Tests that an identity stored in the prefs with no private key nor
// certificate can be properly loaded into memory and returned.
TEST_F(PrefsCertificateStoreTest, GetIdentity_MissingKeyAndCertSuccess) {
  base::Value::Dict identity_dict;
  identity_dict.Set("test", 1);
  prefs_.SetDict(kTestIdentityName, std::move(identity_dict));

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_FALSE(identity->private_key);
  EXPECT_FALSE(identity->certificate);
}

// Tests that an identity stored in the prefs with no details
// can not be properly loaded into memory and returned.
TEST_F(PrefsCertificateStoreTest, GetIdentity_NoDetails_Fails) {
  prefs_.SetDict(kTestIdentityName, base::Value::Dict());

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);
}

// Tests that attempting to retrieve an identity using an invalid identity name
// does not return an actual identity.
TEST_F(PrefsCertificateStoreTest, GetIdentity_InvalidIdentityName_Fails) {
  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kOtherTestIdentityName, test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);
}

// Tests that attempting to retrieve an identity when failing to load a private
// key into memory does not return an actual identity.
TEST_F(PrefsCertificateStoreTest, GetIdentity_LoadPrivateKeyFailure) {
  auto test_cert = LoadTestCert();
  prefs_.SetDict(kTestIdentityName, CreateValidIdentity(*test_cert));

  EXPECT_CALL(*mock_key_factory_, LoadPrivateKeyFromDict(_, _))
      .WillOnce(RunOnceCallback<1>(nullptr));

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kLoadKeyFailed));
}

TEST_F(PrefsCertificateStoreTest, DeleteIdentities_Success) {
  base::Value::Dict identity;
  identity.Set(kCertificate, "some_cert");
  prefs_.SetDict(kTestIdentityName, identity.Clone());
  prefs_.SetDict(kOtherTestIdentityName, identity.Clone());
  ASSERT_TRUE(prefs_.HasPrefPath(kTestIdentityName));
  ASSERT_TRUE(prefs_.HasPrefPath(kOtherTestIdentityName));

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->DeleteIdentities({kTestIdentityName, kOtherTestIdentityName},
                           test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);
  EXPECT_FALSE(prefs_.HasPrefPath(kTestIdentityName));
  EXPECT_FALSE(prefs_.HasPrefPath(kOtherTestIdentityName));
}

TEST_F(PrefsCertificateStoreTest, DeleteIdentities_NotFound) {
  ASSERT_FALSE(prefs_.HasPrefPath(kTestIdentityName));

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->DeleteIdentities({kTestIdentityName}, test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), std::nullopt);
  EXPECT_FALSE(prefs_.HasPrefPath(kTestIdentityName));
}

TEST_F(PrefsCertificateStoreTest, DeleteIdentities_InvalidName) {
  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->DeleteIdentities({kTestIdentityName, ""}, test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidIdentityName);
}

}  // namespace client_certificates
