// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_store.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/mock_private_key.h"
#include "components/enterprise/client_certificates/core/mock_private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/fake_db.h"
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

client_certificates_pb::PrivateKey CreateFakeProtoKey() {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(
      client_certificates_pb::PrivateKey::PRIVATE_UNEXPORTABLE_KEY);
  private_key.set_wrapped_key(kFakeWrappedValue);
  return private_key;
}

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

void PersistCertificate(client_certificates_pb::ClientIdentity& identity,
                        scoped_refptr<net::X509Certificate> certificate) {
  ASSERT_TRUE(certificate);
  base::Pickle pickle;
  certificate->Persist(&pickle);
  *identity.mutable_certificate() =
      std::string(pickle.data_as_char(), pickle.size());
}

MATCHER_P(EqualsProto, expected, "") {
  return arg.SerializeAsString() == expected.SerializeAsString();
}

}  // namespace

using base::test::RunOnceCallback;
using InitStatus = leveldb_proto::Enums::InitStatus;
using base::test::ErrorIs;
using base::test::ValueIs;
using testing::_;
using testing::Return;
using testing::StrictMock;

class CertificateStoreTest : public testing::Test {
 protected:
  using ProtoMap =
      std::map<std::string, client_certificates_pb::ClientIdentity>;

  CertificateStoreTest() {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<client_certificates_pb::ClientIdentity>>(
        &database_entries_);
    fake_db_ = db.get();

    auto mock_key_factory =
        std::make_unique<StrictMock<MockPrivateKeyFactory>>();
    mock_key_factory_ = mock_key_factory.get();

    store_ = CertificateStore::CreateForTesting(std::move(db),
                                                std::move(mock_key_factory));
  }

  void AddDatabaseEntry(const std::string& key,
                        const client_certificates_pb::ClientIdentity& value) {
    database_entries_.insert_or_assign(key, value);
  }

  void RemoveDatabaseEntry(const std::string& key) {
    database_entries_.erase(key);
  }

  void ExpectDatabaseEntry(
      const std::string& key,
      const client_certificates_pb::ClientIdentity& value) {
    auto iterator = database_entries_.find(key);
    ASSERT_TRUE(iterator != database_entries_.end());

    std::string str1, str2;
    EXPECT_TRUE(value.SerializeToString(&str1));
    EXPECT_TRUE(iterator->second.SerializeToString(&str2));
    EXPECT_EQ(str1, str2) << "Expected ClientIdentity proto is different from "
                             "the one saved in the database.";
  }

  void ExpectNoDatabaseEntry(const std::string& key) {
    auto iterator = database_entries_.find(key);
    EXPECT_TRUE(iterator == database_entries_.end());
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<CertificateStore> store_;

  ProtoMap database_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<client_certificates_pb::ClientIdentity>>
      fake_db_;
  raw_ptr<MockPrivateKeyFactory> mock_key_factory_;
};

// Tests the success path of the CreatePrivateKey function:
// - Database initialization succeeds,
// - Parameters are valid,
// - There is no preexisting valid identity with the same name,
// - Private key creation succeeds,
// - Private key serialization succeeds.
TEST_F(CertificateStoreTest, CreatePrivateKey_Success) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mocked_private_key, ToProto()).WillOnce(Return(proto_key));

  EXPECT_CALL(*mock_key_factory_, CreatePrivateKey(_))
      .WillOnce(RunOnceCallback<0>(mocked_private_key));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/true);

  EXPECT_THAT(test_future.Get(), ValueIs(mocked_private_key));

  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  ExpectDatabaseEntry(kTestIdentityName, proto_identity);
}

// Tests that no key is returned when given an invalid identity name.
TEST_F(CertificateStoreTest, CreatePrivateKey_InvalidIdentityNameFail) {
  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey("", test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kInvalidIdentityName));
  ExpectNoDatabaseEntry("");
}

// Tests that no key is returned when failing to initialize the database.
TEST_F(CertificateStoreTest, CreatePrivateKey_DatabaseInitFail) {
  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kCorrupt);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kInvalidDatabaseState));
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that no key is returned when failing to verify that the database
// doesn't already have an identity with the same name.
TEST_F(CertificateStoreTest, CreatePrivateKey_GetIdentityFail) {
  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/false);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kGetDatabaseEntryFailed));
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that no key is returned and the database is not modified when
// the create key call fails.
TEST_F(CertificateStoreTest, CreatePrivateKey_CreateKeyFail) {
  EXPECT_CALL(*mock_key_factory_, CreatePrivateKey(_))
      .WillOnce(RunOnceCallback<0>(nullptr));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kCreateKeyFailed));
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that no key is returned and the database is not modified when
// attempting to create a private key using an identity name that is already
// used.
TEST_F(CertificateStoreTest, CreatePrivateKey_ConflictFail) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();
  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kConflictingIdentity));
  ExpectDatabaseEntry(kTestIdentityName, proto_identity);
}

// Tests that no key is returned when failing to update the database.
TEST_F(CertificateStoreTest, CreatePrivateKey_UpdateFail) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mocked_private_key, ToProto()).WillOnce(Return(proto_key));

  EXPECT_CALL(*mock_key_factory_, CreatePrivateKey(_))
      .WillOnce(RunOnceCallback<0>(mocked_private_key));

  base::test::TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> test_future;
  store_->CreatePrivateKey(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/false);

  // Not checking the mocked database's state, as the mock behaves as if the
  // update succeeded, but in this test case it was mocked to fail - so we can
  // assume it failed.
  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kSaveKeyFailed));
}

// Tests that a certificate can be saved to the database when a private key
// already exists in the database.
TEST_F(CertificateStoreTest, CommitCertificate_SuccessWithPrivateKey) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();
  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), std::nullopt);

  PersistCertificate(proto_identity, test_cert);
  ExpectDatabaseEntry(kTestIdentityName, proto_identity);
}

// Tests that a certificate can be saved to the database even when a private key
// does not already exist in the database.
TEST_F(CertificateStoreTest, CommitCertificate_SuccessWithoutPrivateKey) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), std::nullopt);

  client_certificates_pb::ClientIdentity proto_identity;
  PersistCertificate(proto_identity, test_cert);
  ExpectDatabaseEntry(kTestIdentityName, proto_identity);
}

// Tests that a certificate won't be saved to the database when the identity
// name is invalid.
TEST_F(CertificateStoreTest, CommitCertificate_InvalidIdentityNameFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate("", test_cert, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidIdentityName);
  ExpectNoDatabaseEntry("");
}

// Tests that a certificate won't be saved to the database when the database
// initialization failed.
TEST_F(CertificateStoreTest, CommitCertificate_DatabaseInitFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kCorrupt);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidDatabaseState);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when failing to "get"
// from the database.
TEST_F(CertificateStoreTest, CommitCertificate_GetIdentityFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/false);

  EXPECT_EQ(test_future.Get(), StoreError::kGetDatabaseEntryFailed);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when the given
// certificate instance is invalid (nullptr).
TEST_F(CertificateStoreTest, CommitCertificate_InvalidCertificateFail) {
  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, /*certificate=*/nullptr,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidCertificateInput);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when failing to
// "update" the database.
TEST_F(CertificateStoreTest, CommitCertificate_UpdateFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitCertificate(kTestIdentityName, test_cert,
                            test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/false);

  // Not checking the mocked database's state, as the mock behaves as if the
  // update succeeded, but in this test case it was mocked to fail - so we can
  // assume it failed.
  EXPECT_EQ(test_future.Get(), StoreError::kCertificateCommitFailed);
}

// Tests that an existing private key is moved and a certificate can be saved
// to the database.
TEST_F(CertificateStoreTest, CommitIdentity_SuccessWithPrivateKey) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();
  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kOtherTestIdentityName, proto_identity);

  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), std::nullopt);

  PersistCertificate(proto_identity, test_cert);
  ExpectDatabaseEntry(kTestIdentityName, proto_identity);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
}

// Tests that a certificate cannot be saved to the database when the temporary
// identity does not already exist in the database.
TEST_F(CertificateStoreTest, CommitIdentity_FailWithoutIdentity) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), StoreError::kIdentityNotFound);

  ExpectNoDatabaseEntry(kTestIdentityName);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
}

// Tests that a certificate won't be saved to the database when the temporary
// identity name is invalid.
TEST_F(CertificateStoreTest, CommitIdentity_InvalidTemporaryIdentityNameFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity("", kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidIdentityName);
  ExpectNoDatabaseEntry("");
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when the database
// initialization failed.
TEST_F(CertificateStoreTest, CommitIdentity_DatabaseInitFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kCorrupt);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidDatabaseState);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when failing to "get"
// from the database.
TEST_F(CertificateStoreTest, CommitIdentity_GetIdentityFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/false);

  EXPECT_EQ(test_future.Get(), StoreError::kGetDatabaseEntryFailed);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when the given
// certificate instance is invalid (nullptr).
TEST_F(CertificateStoreTest, CommitIdentity_InvalidCertificateFail) {
  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName,
                         /*certificate=*/nullptr, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidCertificateInput);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when the given
// certificate instance is invalid (nullptr).
TEST_F(CertificateStoreTest, CommitIdentity_InvalidFinalNameFail) {
  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, "", test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_EQ(test_future.Get(), StoreError::kInvalidFinalIdentityName);
  ExpectNoDatabaseEntry(kOtherTestIdentityName);
  ExpectNoDatabaseEntry(kTestIdentityName);
}

// Tests that a certificate won't be saved to the database when failing to
// "update" the database.
TEST_F(CertificateStoreTest, CommitIdentity_UpdateFail) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();
  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kOtherTestIdentityName, proto_identity);

  auto test_cert = LoadTestCert();

  base::test::TestFuture<std::optional<StoreError>> test_future;
  store_->CommitIdentity(kOtherTestIdentityName, kTestIdentityName, test_cert,
                         test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);
  fake_db_->UpdateCallback(/*success=*/false);

  // Not checking the mocked database's state, as the mock behaves as if the
  // update succeeded, but in this test case it was mocked to fail - so we can
  // assume it failed.
  EXPECT_EQ(test_future.Get(), StoreError::kCertificateCommitFailed);
}

// Tests that an identity stored in the database with a private key and
// certificate can be properly loaded into memory and returned.
TEST_F(CertificateStoreTest, GetIdentity_FullIdentitySuccess) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mock_key_factory_, LoadPrivateKey(EqualsProto(proto_key), _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto test_cert = LoadTestCert();

  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  PersistCertificate(proto_identity, test_cert);
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_EQ(identity->private_key, mocked_private_key);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(identity->certificate.get()));
}

// Tests that an identity stored in the database with only a private key can be
// properly loaded into memory and returned.
TEST_F(CertificateStoreTest, GetIdentity_OnlyPrivateKeySuccess) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*mock_key_factory_, LoadPrivateKey(EqualsProto(proto_key), _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_EQ(identity->private_key, mocked_private_key);
  EXPECT_FALSE(identity->certificate);
}

// Tests that an identity stored in the database with only a certificate can be
// properly loaded into memory and returned.
TEST_F(CertificateStoreTest, GetIdentity_OnlyCertificateSuccess) {
  auto test_cert = LoadTestCert();

  client_certificates_pb::ClientIdentity proto_identity;
  PersistCertificate(proto_identity, test_cert);
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_FALSE(identity->private_key);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(identity->certificate.get()));
}

// Tests that an identity stored in the database with no private key nor
// certificate can be properly loaded into memory and returned.
TEST_F(CertificateStoreTest, GetIdentity_EmptySuccess) {
  client_certificates_pb::ClientIdentity proto_identity;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->name, kTestIdentityName);
  EXPECT_FALSE(identity->private_key);
  EXPECT_FALSE(identity->certificate);
}

// Tests that attempting to retrieve an identity using an unknown identity name
// does not return an actual identity.
TEST_F(CertificateStoreTest, GetIdentity_NotFoundSuccess) {
  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  ASSERT_OK_AND_ASSIGN(std::optional<ClientIdentity> identity,
                       test_future.Get());
  EXPECT_FALSE(identity.has_value());
}

// Tests that attempting to retrieve an identity using an invalid identity name
// does not return an actual identity.
TEST_F(CertificateStoreTest, GetIdentity_InvalidIdentityNameFail) {
  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity("", test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kInvalidIdentityName));
}

// Tests that attempting to retrieve an identity when the database failed to
// initialize does not return an actual identity.
TEST_F(CertificateStoreTest, GetIdentity_DatabaseInitFail) {
  client_certificates_pb::ClientIdentity proto_identity;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kCorrupt);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kInvalidDatabaseState));
}

// Tests that attempting to retrieve an identity when the database failed the
// "get" request does not return an actual identity.
TEST_F(CertificateStoreTest, GetIdentity_GetIdentityFail) {
  client_certificates_pb::ClientIdentity proto_identity;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/false);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kGetDatabaseEntryFailed));
}

// Tests that attempting to retrieve an identity when failing to load a private
// key into memory does not return an actual identity.
TEST_F(CertificateStoreTest, GetIdentity_LoadPrivateKeyFail) {
  client_certificates_pb::PrivateKey proto_key = CreateFakeProtoKey();

  EXPECT_CALL(*mock_key_factory_, LoadPrivateKey(EqualsProto(proto_key), _))
      .WillOnce(RunOnceCallback<1>(nullptr));

  client_certificates_pb::ClientIdentity proto_identity;
  *proto_identity.mutable_private_key() = proto_key;
  AddDatabaseEntry(kTestIdentityName, proto_identity);

  base::test::TestFuture<StoreErrorOr<std::optional<ClientIdentity>>>
      test_future;
  store_->GetIdentity(kTestIdentityName, test_future.GetCallback());

  fake_db_->InitStatusCallback(InitStatus::kOK);
  fake_db_->GetCallback(/*success=*/true);

  EXPECT_THAT(test_future.Get(), ErrorIs(StoreError::kLoadKeyFailed));
}

}  // namespace client_certificates
