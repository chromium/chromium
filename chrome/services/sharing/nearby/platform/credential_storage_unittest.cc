// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/credential_storage.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_callbacks.h"

namespace {

const char kManagerAppName[] = "test_manager_app_id";
const char kAccountName[] = "test_account_name";

const std::vector<uint8_t> kSecretId_1 = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
const std::vector<uint8_t> kKeySeed_1 = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
const std::vector<uint8_t> kMetadataEncryptionKeyV0_1 = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e};
constexpr int64_t kStartTimeMillis_1 = 255486129307;
const char AdvertisementSigningKeyCertificateAlias_1[] =
    "NearbySharingABCDEF123456";
const std::vector<uint8_t> kAdvertisementPrivateKey_1 = {0x41, 0x42, 0x43,
                                                         0x44, 0x45, 0x46};
const char ConnectionSigningKeyCertificateAlias_1[] = "NearbySharingXYZ789";
const std::vector<uint8_t> kConnectionPrivateKey_1 = {0x51, 0x52, 0x53,
                                                      0x54, 0x55, 0x56};
const std::vector<uint8_t> kMetadataEncryptionKeyV1_1 = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70};
const base::flat_map<uint32_t, bool> kConsumedSalts_1 = {{0xb412, true},
                                                         {0x34b2, false},
                                                         {0x5171, false}};

::nearby::internal::LocalCredential CreateLocalCredentialProto(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    int64_t start_time_millis,
    const std::vector<uint8_t>& metadata_encryption_key_v0,
    const std::string& advertisement_signing_key_certificate_alias,
    const std::vector<uint8_t>& advertisement_private_key,
    const std::string& connection_signing_key_certificate_alias,
    const std::vector<uint8_t>& connection_private_key,
    const base::flat_map<uint32_t, bool>& consumed_salts,
    const std::vector<uint8_t>& metadata_encryption_key_v1) {
  ::nearby::internal::LocalCredential proto;

  proto.set_secret_id(std::string(secret_id.begin(), secret_id.end()));
  proto.set_key_seed(std::string(key_seed.begin(), key_seed.end()));
  proto.set_start_time_millis(start_time_millis);
  proto.set_metadata_encryption_key_v0(std::string(
      metadata_encryption_key_v0.begin(), metadata_encryption_key_v0.end()));

  proto.mutable_advertisement_signing_key()->set_certificate_alias(
      advertisement_signing_key_certificate_alias);
  proto.mutable_advertisement_signing_key()->set_key(std::string(
      advertisement_private_key.begin(), advertisement_private_key.end()));

  proto.mutable_connection_signing_key()->set_certificate_alias(
      connection_signing_key_certificate_alias);
  proto.mutable_connection_signing_key()->set_key(std::string(
      connection_private_key.begin(), connection_private_key.end()));

  // All local credentials have IdentityType of kIdentityTypePrivate.
  proto.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);

  for (const auto& pair : consumed_salts) {
    auto map_pair =
        ::google::protobuf::MapPair<uint32_t, bool>(pair.first, pair.second);
    proto.mutable_consumed_salts()->insert(map_pair);
  }

  proto.set_metadata_encryption_key_v1(std::string(
      metadata_encryption_key_v1.begin(), metadata_encryption_key_v1.end()));

  return proto;
}

class FakeNearbyPresenceCredentialStorage
    : public ash::nearby::presence::mojom::NearbyPresenceCredentialStorage {
 public:
  FakeNearbyPresenceCredentialStorage() = default;
  ~FakeNearbyPresenceCredentialStorage() override = default;

  // mojom::NearbyPresenceCredentialStorage:
  void SaveCredentials(
      std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
          local_credentials,
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage::
          SaveCredentialsCallback callback) override {
    if (should_credentials_successfully_save_) {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk);
    } else {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kUnknown);
    }
  }

  void SetShouldCredentialsSuccessfullySave(bool should_succeed) {
    should_credentials_successfully_save_ = should_succeed;
  }

 private:
  bool should_credentials_successfully_save_ = true;
};

}  // namespace

namespace nearby::chrome {

class CredentialStorageTest : public testing::Test {
 public:
  CredentialStorageTest() = default;

  ~CredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto fake_credential_storage =
        std::make_unique<FakeNearbyPresenceCredentialStorage>();
    fake_credential_storage_ = fake_credential_storage.get();

    mojo::PendingRemote<
        ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
        pending_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_credential_storage),
        pending_remote.InitWithNewPipeAndPassReceiver());

    remote_credential_storage_.Bind(std::move(pending_remote),
                                    /*bind_task_runner=*/nullptr);

    credential_storage_ =
        std::make_unique<CredentialStorage>(remote_credential_storage_);
  }

  void TearDown() override {
    // Prevent dangling raw pointer once 'remote_credential_storage_' is
    // deconstructed.
    fake_credential_storage_ = nullptr;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  mojo::SharedRemote<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      remote_credential_storage_;
  std::unique_ptr<CredentialStorage> credential_storage_;
  raw_ptr<FakeNearbyPresenceCredentialStorage> fake_credential_storage_;
};

TEST_F(CredentialStorageTest, Initialize) {
  EXPECT_TRUE(credential_storage_);
}

TEST_F(CredentialStorageTest, SaveCredentials_Succeed) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;

  local_credentials.emplace_back(CreateLocalCredentialProto(
      kSecretId_1, kKeySeed_1, kStartTimeMillis_1, kMetadataEncryptionKeyV0_1,
      AdvertisementSigningKeyCertificateAlias_1, kAdvertisementPrivateKey_1,
      ConnectionSigningKeyCertificateAlias_1, kConnectionPrivateKey_1,
      kConsumedSalts_1, kMetadataEncryptionKeyV1_1));

  // TODO(b/287334195): Populate and save public credentials once
  // CredentialStorage has public credential support.
  std::vector<::nearby::internal::SharedCredential> shared_credentials;

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_TRUE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/true);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      std::move(callback));

  run_loop.Run();
}

TEST_F(CredentialStorageTest, SaveCredentials_Fail) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;

  local_credentials.emplace_back(CreateLocalCredentialProto(
      kSecretId_1, kKeySeed_1, kStartTimeMillis_1, kMetadataEncryptionKeyV0_1,
      AdvertisementSigningKeyCertificateAlias_1, kAdvertisementPrivateKey_1,
      ConnectionSigningKeyCertificateAlias_1, kConnectionPrivateKey_1,
      kConsumedSalts_1, kMetadataEncryptionKeyV1_1));

  // TODO(b/287334195): Populate and save public credentials once
  // CredentialStorage has public credential support.
  std::vector<::nearby::internal::SharedCredential> shared_credentials;

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_FALSE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/false);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      std::move(callback));

  run_loop.Run();
}

}  // namespace nearby::chrome
