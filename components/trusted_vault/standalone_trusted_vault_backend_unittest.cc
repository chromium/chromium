// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_backend.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/mock_recovery_key_store_connection.h"
#include "components/trusted_vault/test/mock_trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::ByMove;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::Ne;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

MATCHER_P(MatchTrustedVaultKeyAndVersions, expected, "") {
  const auto* trusted_vault_keys =
      absl::get_if<std::vector<TrustedVaultKeyAndVersion>>(&arg);
  if (!trusted_vault_keys) {
    *result_listener << "does not hold a vector of TrustedVaultKeyAndVersion";
    return false;
  }
  return testing::ExplainMatchResult(*trusted_vault_keys, expected,
                                     result_listener);
}

MATCHER_P(DegradedRecoverabilityStateEq, expected_state, "") {
  const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
      given_state = arg;
  return given_state.degraded_recoverability_value() ==
             expected_state.degraded_recoverability_value() &&
         given_state.last_refresh_time_millis_since_unix_epoch() ==
             expected_state.last_refresh_time_millis_since_unix_epoch();
}

MATCHER_P(KeyMaterialEq, expected, "") {
  const std::string& key_material = arg.key_material();
  const std::vector<uint8_t> key_material_as_bytes(key_material.begin(),
                                                   key_material.end());
  return key_material_as_bytes == expected;
}

MATCHER_P2(TrustedVaultKeyAndVersionEq, expected_key, expected_version, "") {
  const TrustedVaultKeyAndVersion& key_and_version = arg;
  return key_and_version.key == expected_key &&
         key_and_version.version == expected_version;
}

MATCHER_P(PublicKeyWhenExportedEq, expected, "") {
  const SecureBoxPublicKey& actual_public_key = arg;
  return actual_public_key.ExportToBytes() == expected;
}

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

CoreAccountInfo MakeAccountInfoWithGaiaId(const std::string& gaia_id) {
  CoreAccountInfo account_info;
  account_info.gaia = gaia_id;
  return account_info;
}

bool WriteLocalTrustedVaultFile(
    const trusted_vault_pb::LocalTrustedVault& proto,
    const base::FilePath& path) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(proto.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      base::MD5String(file_proto.serialized_local_trusted_vault()));
  return base::WriteFile(path, file_proto.SerializeAsString());
}

trusted_vault_pb::LocalTrustedVault ReadLocalTrustedVaultFile(
    const base::FilePath& path) {
  std::string file_content;
  trusted_vault_pb::LocalTrustedVault data_proto;
  if (!base::ReadFileToString(path, &file_content)) {
    return data_proto;
  }
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  if (!file_proto.ParseFromString(file_content)) {
    return data_proto;
  }

  if (base::MD5String(file_proto.serialized_local_trusted_vault()) !=
      file_proto.md5_digest_hex_string()) {
    return data_proto;
  }

  data_proto.ParseFromString(file_proto.serialized_local_trusted_vault());
  return data_proto;
}

class MockDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void, NotifyRecoverabilityDegradedChanged, (), (override));
  MOCK_METHOD(void, NotifyStateChanged, (), (override));
};

class FakeRecoveryKeyProvider
    : public RecoveryKeyStoreController::RecoveryKeyProvider {
 public:
  explicit FakeRecoveryKeyProvider(std::vector<uint8_t> public_key_bytes)
      : public_key_bytes_(std::move(public_key_bytes)) {
    CHECK(SecureBoxPublicKey::CreateByImport(public_key_bytes_))
        << "public_key must be valid";
  }

  void GetCurrentRecoveryKeyStoreData(
      RecoveryKeyStoreDataCallback callback) override {
    trusted_vault_pb::Vault vault;
    vault.set_vault_metadata("test vault metadata");
    vault.set_recovery_key("test recovery key");
    trusted_vault_pb::ApplicationKey* application_key =
        vault.add_application_keys();
    application_key->set_key_name(
        "security_domain_member_key_encrypted_locally");
    application_key->mutable_asymmetric_key_pair()->set_public_key(
        public_key_bytes_.data(), public_key_bytes_.size());
    std::move(callback).Run(std::move(vault));
  }

 private:
  const std::vector<uint8_t> public_key_bytes_;
};

class StandaloneTrustedVaultBackendTest : public testing::Test {
 public:
  StandaloneTrustedVaultBackendTest()
      : file_path_(
            CreateUniqueTempDir(&temp_dir_)
                .Append(base::FilePath(FILE_PATH_LITERAL("some_file")))) {
    clock_.SetNow(base::Time::Now());
    ResetBackend();
  }

  ~StandaloneTrustedVaultBackendTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  void ResetBackend() {
    auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();

    auto connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    connection_ = connection.get();

    if (recovery_key_provider_holder_) {
      auto recovery_key_store_connection =
          std::make_unique<testing::NiceMock<MockRecoveryKeyStoreConnection>>();
      recovery_key_store_connection_ = recovery_key_store_connection.get();
      backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
          security_domain_id(), file_path_, std::move(delegate),
          std::move(connection), std::move(recovery_key_provider_holder_),
          std::move(recovery_key_store_connection));
    } else {
      backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
          security_domain_id(), file_path_, std::move(delegate),
          std::move(connection));
    }
    backend_->SetClockForTesting(&clock_);
    backend_->ReadDataFromDisk();

    // To avoid DCHECK failures in tests that exercise SetPrimaryAccount(),
    // return non-null for RegisterAuthenticationFactor(). This registration
    // operation will never complete, though.
    ON_CALL(*connection_, RegisterAuthenticationFactor)
        .WillByDefault(testing::InvokeWithoutArgs([&]() {
          return std::make_unique<TrustedVaultConnection::Request>();
        }));
    ON_CALL(*connection_, RegisterLocalDeviceWithoutKeys)
        .WillByDefault(testing::InvokeWithoutArgs([&]() {
          return std::make_unique<TrustedVaultConnection::Request>();
        }));
  }

  MockTrustedVaultConnection* connection() { return connection_; }

  base::SimpleTestClock* clock() { return &clock_; }

  StandaloneTrustedVaultBackend* backend() { return backend_.get(); }

  SecurityDomainId security_domain_id() const {
    return SecurityDomainId::kChromeSync;
  }

  std::string security_domain_name_for_uma() const {
    return GetSecurityDomainNameForUma(security_domain_id());
  }

  const base::FilePath& file_path() { return file_path_; }

  void SetPrimaryAccountWithUnknownAuthError(
      std::optional<CoreAccountInfo> primary_account) {
    backend_->SetPrimaryAccount(
        primary_account,
        StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown);
  }

  // Stores |vault_keys| and mimics successful device registration, returns
  // private device key material.
  std::vector<uint8_t> StoreKeysAndMimicDeviceRegistration(
      const std::vector<std::vector<uint8_t>>& vault_keys,
      int last_vault_key_version,
      CoreAccountInfo account_info) {
    DCHECK(!vault_keys.empty());
    backend_->StoreKeys(account_info.gaia, vault_keys, last_vault_key_version);
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        device_registration_callback;

    EXPECT_CALL(
        *connection_,
        RegisterAuthenticationFactor(
            Eq(account_info),
            MatchTrustedVaultKeyAndVersions(GetTrustedVaultKeysWithVersions(
                vault_keys, last_vault_key_version)),
            _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                const MemberKeysSource& member_keys_source,
                const SecureBoxPublicKey& device_public_key,
                AuthenticationFactorType,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              device_registration_callback = std::move(callback);
              // Note: TrustedVaultConnection::Request doesn't support
              // cancellation, so these tests don't cover the contract that
              // caller should store Request object until it's completed or need
              // to be cancelled.
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    // Setting the primary account will trigger device registration.
    SetPrimaryAccountWithUnknownAuthError(account_info);
    Mock::VerifyAndClearExpectations(connection_);
    EXPECT_FALSE(device_registration_callback.is_null());

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess,
             /*key_version=*/last_vault_key_version);

    // Reset primary account.
    SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);

    std::string device_private_key_material =
        backend_->GetDeviceRegistrationInfoForTesting(account_info.gaia)
            .private_key_material();
    return std::vector<uint8_t>(device_private_key_material.begin(),
                                device_private_key_material.end());
  }

  MockRecoveryKeyStoreConnection* recovery_key_store_connection() {
    return recovery_key_store_connection_;
  }

  void SetUpRecoveryKey(const std::vector<uint8_t>& public_key_bytes) {
    recovery_key_provider_holder_ =
        std::make_unique<FakeRecoveryKeyProvider>(public_key_bytes);
  }

 private:
  base::ScopedTempDir temp_dir_;
  const base::FilePath file_path_;
  base::SimpleTestClock clock_;
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;
  raw_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection_ = nullptr;

  std::unique_ptr<FakeRecoveryKeyProvider> recovery_key_provider_holder_;
  raw_ptr<testing::NiceMock<MockRecoveryKeyStoreConnection>>
      recovery_key_store_connection_ = nullptr;
};

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldWriteDegradedRecoverabilityState) {
  SetPrimaryAccountWithUnknownAuthError(MakeAccountInfoWithGaiaId("user"));
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      123);
  backend()->WriteDegradedRecoverabilityState(degraded_recoverability_state);

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(1));
  EXPECT_THAT(proto.user(0).degraded_recoverability_state(),
              DegradedRecoverabilityStateEq(degraded_recoverability_state));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldInvokeGetIsRecoverabilityDegradedCallbackImmediately) {
  // The TaskEnvironment is needed because this test initializes the handler,
  // which works with time.
  base::test::SingleThreadTaskEnvironment environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  SetPrimaryAccountWithUnknownAuthError(account_info);
  EXPECT_CALL(*connection(), DownloadIsRecoverabilityDegraded)
      .WillOnce([](const CoreAccountInfo&,
                   TrustedVaultConnection::IsRecoverabilityDegradedCallback
                       callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  base::MockCallback<base::OnceCallback<void(bool)>> cb;
  // The callback should be invoked because GetIsRecoverabilityDegraded() is
  // called with the current primary account.
  EXPECT_CALL(cb, Run(true));
  backend()->GetIsRecoverabilityDegraded(account_info, cb.Get());
  environment.FastForwardBy(base::Milliseconds(1));
}

TEST_F(
    StandaloneTrustedVaultBackendTest,
    ShouldDeferGetIsRecoverabilityDegradedCallbackUntilSetPrimaryAccountIsInvoked) {
  // TODO(crbug.com/40255601): looks like this test verifies scenario not
  // possible in prod anymore, remove it together with
  // |pending_get_is_recoverability_degraded_| logic.

  // The TaskEnvironment is needed because this test initializes the handler,
  // which works with time.
  base::test::SingleThreadTaskEnvironment environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SetPrimaryAccountWithUnknownAuthError(MakeAccountInfoWithGaiaId("user1"));

  base::MockCallback<base::OnceCallback<void(bool)>> cb;
  // The callback should not be invoked because GetIsRecoverabilityDegraded()
  // and SetPrimaryAccount() are invoked with different accounts.
  EXPECT_CALL(cb, Run(_)).Times(0);
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user2");
  // This GetIsRecoverabilityDegraded() is corresponding to a late
  // SetPrimaryAccount(), in this case the callback should be deferred and
  // invoked when SetPrimaryAccount() is called.
  backend()->GetIsRecoverabilityDegraded(account_info, cb.Get());

  Mock::VerifyAndClearExpectations(&cb);

  ON_CALL(*connection(), DownloadIsRecoverabilityDegraded(Eq(account_info), _))
      .WillByDefault([](const CoreAccountInfo&,
                        TrustedVaultConnection::IsRecoverabilityDegradedCallback
                            callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // The callback should be invoked on SetPrimaryAccount() since the last
  // GetIsRecoverabilityDegraded() was called with the same account.
  EXPECT_CALL(cb, Run(true));
  SetPrimaryAccountWithUnknownAuthError(account_info);
  environment.FastForwardBy(base::Milliseconds(1));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotInvokeGetIsRecoverabilityDegradedCallback) {
  // TODO(crbug.com/40255601): looks like this test verifies scenario not
  // possible in prod anymore, remove it together with
  // |pending_get_is_recoverability_degraded_| logic.

  // The TaskEnvironment is needed because this test initializes the handler,
  // which works with time.
  base::test::SingleThreadTaskEnvironment environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  EXPECT_CALL(*connection(), DownloadIsRecoverabilityDegraded).Times(0);
  base::MockCallback<base::OnceCallback<void(bool)>> cb;
  // The callback should not be invoked because GetIsRecoverabilityDegraded()
  // and SetPrimaryAccount() are invoked with different accounts.
  EXPECT_CALL(cb, Run(_)).Times(0);
  backend()->GetIsRecoverabilityDegraded(MakeAccountInfoWithGaiaId("user1"),
                                         cb.Get());

  SetPrimaryAccountWithUnknownAuthError(MakeAccountInfoWithGaiaId("user2"));
  environment.FastForwardBy(base::Milliseconds(1));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchEmptyKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRecordNotFoundWhenReadingFile) {
  base::HistogramTester histogram_tester;
  backend()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kNotFound,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRecordMD5DigestMismatchWhenReadingFile) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_md5_digest_hex_string("corrupted_md5_digest");
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  base::HistogramTester histogram_tester;
  backend()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kMD5DigestMismatch,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRecordFileProtoDeserializationFailedWhenReadingFile) {
  ASSERT_TRUE(base::WriteFile(file_path(), "corrupted_proto"));

  base::HistogramTester histogram_tester;
  backend()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultFileReadStatusForUMA::kFileProtoDeserializationFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRecordDataProtoDeserializationFailedWhenReadingFile) {
  const std::string kCorruptedSerializedDataProto = "corrupted_proto";
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(kCorruptedSerializedDataProto);
  file_proto.set_md5_digest_hex_string(
      base::MD5String(kCorruptedSerializedDataProto));
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  base::HistogramTester histogram_tester;
  backend()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultFileReadStatusForUMA::kDataProtoDeserializationFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldReadAndFetchNonEmptyKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(account_info_1.gaia);
  user_data2->set_gaia_id(account_info_2.gaia);
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data2->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  base::HistogramTester histogram_tester;
  backend()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  backend()->FetchKeys(account_info_1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  backend()->FetchKeys(account_info_2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFilterOutConstantKey) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data =
      initial_data.add_user();
  user_data->set_gaia_id(account_info.gaia);
  user_data->add_vault_key()->set_key_material(
      GetConstantTrustedVaultKey().data(), GetConstantTrustedVaultKey().size());
  user_data->add_vault_key()->set_key_material(kKey.data(), kKey.size());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  backend()->ReadDataFromDisk();

  // Keys should be fetched immediately, constant key must be filtered out.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldStoreKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  base::HistogramTester histogram_tester;
  backend()->StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/7);
  backend()->StoreKeys(kGaiaId2, {kKey2}, /*last_key_version=*/8);
  // Keys for |kGaiaId2| overridden, so |kKey2| should be lost.
  backend()->StoreKeys(kGaiaId2, {kKey3, kKey4}, /*last_key_version=*/9);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileWriteSuccess." + security_domain_name_for_uma(),
      /*sample=*/true,
      /*expected_bucket_count=*/3);

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(2));
  EXPECT_THAT(proto.user(0).vault_key(), ElementsAre(KeyMaterialEq(kKey1)));
  EXPECT_THAT(proto.user(0).last_vault_key_version(), Eq(7));
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(kKey3), KeyMaterialEq(kKey4)));
  EXPECT_THAT(proto.user(1).last_vault_key_version(), Eq(9));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUpgradeToVersion1AndFixMissingConstantKey) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(account_info_1.gaia);
  user_data2->set_gaia_id(account_info_2.gaia);
  // Mimic |user_data1| to be affected by crbug.com/1267391 and |user_data2| to
  // be not affected.
  AssignBytesToProtoString(kKey1,
                           user_data1->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           user_data2->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(kKey2,
                           user_data2->add_vault_key()->mutable_key_material());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  // Backend should fix corrupted data and write new state.
  backend()->ReadDataFromDisk();

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(2));
  // Constant key should be added for the first user.
  EXPECT_THAT(proto.user(0).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey1)));
  // Sanity check that state for the second user isn't changed.
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey2)));
  EXPECT_THAT(proto.data_version(), testing::Ge(1));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUpgradeAllUsersDataToVersion2AndResetKeysAreStale) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(account_info_1.gaia);
  user_data1->set_keys_marked_as_stale_by_consumer(true);
  user_data2->set_gaia_id(account_info_2.gaia);
  user_data2->set_keys_marked_as_stale_by_consumer(true);
  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));

  // Backend should reset |keys_marked_as_stale_by_consumer| for both accounts
  // and write new state.
  backend()->ReadDataFromDisk();

  trusted_vault_pb::LocalTrustedVault new_data =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(new_data.user_size(), Eq(2));
  EXPECT_FALSE(new_data.user(0).keys_marked_as_stale_by_consumer());
  EXPECT_FALSE(new_data.user(1).keys_marked_as_stale_by_consumer());
  EXPECT_THAT(new_data.data_version(), Eq(2));
}

// This test ensures that migration logic in ReadDataFromDisk() doesn't create
// new file if there wasn't any.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldNotWriteEmptyData) {
  backend()->ReadDataFromDisk();
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchPreviouslyStoredKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(account_info_1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(account_info_2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Instantiate a second backend to read the file.
  auto other_backend = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      security_domain_id(), file_path(),
      std::make_unique<testing::NiceMock<MockDelegate>>(),
      std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>());
  other_backend->ReadDataFromDisk();

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  other_backend->FetchKeys(account_info_1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  other_backend->FetchKeys(account_info_2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDeleteNonPrimaryAccountKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(account_info_1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(account_info_2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Make sure that backend handles primary account changes prior
  // UpdateAccountsInCookieJarInfo() call.
  SetPrimaryAccountWithUnknownAuthError(account_info_1);
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);

  // Keys should be removed immediately if account is not primary and not in
  // cookie jar.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());

  // Keys should be removed from both in-memory and disk storages.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_1, fetch_keys_callback.Get());

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_2, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferPrimaryAccountKeysDeletion) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(account_info.gaia, {kKey}, /*last_key_version=*/0);
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Reset primary account, keys should be deleted from both in-memory and disk
  // storage.
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldCompletePrimaryAccountKeysDeletionAfterRestart) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(account_info.gaia, {kKey}, /*last_key_version=*/0);
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Mimic browser restart and reset primary account.
  auto new_backend = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      security_domain_id(), file_path(),
      /*delegate=*/std::make_unique<testing::NiceMock<MockDelegate>>(),
      /*connection=*/nullptr);
  new_backend->ReadDataFromDisk();
  new_backend->SetPrimaryAccount(
      /*primary_account=*/std::nullopt,
      StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown);
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  new_backend->SetPrimaryAccount(
      account_info,
      StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown);
  new_backend->FetchKeys(account_info, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRegisterDevice) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
      /*sample=*/false,
      /*expected_bucket_count=*/1);

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
  histogram_tester.ExpectUniqueSample(
      /*name=*/"TrustedVault.DeviceRegistrationOutcome." +
          security_domain_name_for_uma(),
      /*sample=*/TrustedVaultDeviceRegistrationOutcomeForUMA::kSuccess,
      /*expected_bucket_count=*/1);

  // Now the device should be registered.
  trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(base::as_bytes(
          base::make_span(registration_info.private_key_material())));
  EXPECT_THAT(key_pair->public_key().ExportToBytes(),
              Eq(serialized_public_device_key));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldHandleLocalDataObsoleteAndPersistState) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
      .WillOnce([&](const CoreAccountInfo&,
                    const MemberKeysSource& member_keys_source,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration failed with kLocalDataObsolete.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kLocalDataObsolete,
           /*key_version=*/0);

  // Verify persisted file state.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(1));
  // Ensure that the failure is remembered, so there are no retries. This is a
  // regression test for crbug.com/1358015.
  EXPECT_TRUE(proto.user(0)
                  .local_device_registration_info()
                  .last_registration_returned_local_data_obsolete());
  // Additionally ensure that |local_device_registration_info| has correct
  // state.
  EXPECT_FALSE(
      proto.user(0).local_device_registration_info().device_registered());
  EXPECT_TRUE(proto.user(0)
                  .local_device_registration_info()
                  .has_private_key_material());
  // Keys shouldn't be marked as stale: this is exclusively about upper layers
  // invoking MarkLocalKeysAsStale().
  EXPECT_FALSE(proto.user(0).keys_marked_as_stale_by_consumer());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldClearDataAndAttemptDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kInitialVaultKeys = {{1, 2, 3}};
  const int kInitialLastKeyVersion = 1;

  // Mimic device previously registered with some keys.
  StoreKeysAndMimicDeviceRegistration(kInitialVaultKeys, kInitialLastKeyVersion,
                                      account_info);

  // Set primary account to trigger immediate device registration attempt upon
  // reset.
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // Expect device registration attempt without keys.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(*connection(),
              RegisterLocalDeviceWithoutKeys(Eq(account_info), _, _))
      .WillOnce([&](const CoreAccountInfo& account_info,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Clear data for |account_info|, keys should be removed and device
  // registration attempt should be triggered.
  backend()->ClearLocalDataForAccount(account_info);

  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Mimic successful device registration and verify the state.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess,
           kInitialLastKeyVersion + 1);

  // Now the device should be registered.
  trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(base::as_bytes(
          base::make_span(registration_info.private_key_material())));
  EXPECT_THAT(key_pair->public_key().ExportToBytes(),
              Eq(serialized_public_device_key));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRetryDeviceRegistrationWhenAuthErrorResolved) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _));

  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kPersistentAuthError);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);

  Mock::VerifyAndClearExpectations(connection());

  // When the auth error is resolved, the registration should be retried.
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _));

  base::HistogramTester histogram_tester2;
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kNoPersistentAuthErrors);

  // The second attempt should NOT have logged the histogram, following the
  // histogram's definition that it should be logged once.
  histogram_tester2.ExpectTotalCount(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*expected_count=*/0);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldTryToRegisterDeviceEvenIfLocalKeysAreStale) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(account_info));

  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _));

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(account_info);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotTryToRegisterDeviceIfPreviousAttemptFailed) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  backend()->SetLastRegistrationReturnedLocalDataObsoleteForTesting(
      account_info.gaia);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(account_info);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRegisterDeviceAlthoughPreviousAttemptFailedUponNewStoredKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialKeys = {1, 2, 3};
  const int kInitialKeysVersion = 5;
  const std::vector<uint8_t> kNewKeys = {1, 2, 3, 4};
  const int kNewKeysVersion = 6;

  backend()->StoreKeys(account_info.gaia, {kInitialKeys}, kInitialKeysVersion);
  backend()->SetLastRegistrationReturnedLocalDataObsoleteForTesting(
      account_info.gaia);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);
  SetPrimaryAccountWithUnknownAuthError(account_info);
  Mock::VerifyAndClearExpectations(connection());

  ASSERT_FALSE(backend()
                   ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                   .device_registered());

  // StoreKeys() should trigger a registration nevertheless.
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kNewKeys}, kNewKeysVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _));

  backend()->StoreKeys(account_info.gaia, {kNewKeys}, kNewKeysVersion);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUnthrottleDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const MemberKeysSource&,
              const SecureBoxPublicKey&, AuthenticationFactorType,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kOtherError, /*key_version=*/0);

  // Mimic a restart to trigger device registration attempt, which should remain
  // throttled.
  base::HistogramTester histogram_tester;
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  SetPrimaryAccountWithUnknownAuthError(account_info);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kThrottledClientSide,
      /*expected_bucket_count=*/1);

  // Mimic a restart after sufficient time has passed, to trigger another device
  // registration attempt, which should now be unthrottled.
  base::HistogramTester histogram_tester2;
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  clock()->Advance(StandaloneTrustedVaultBackend::kThrottlingDuration);
  SetPrimaryAccountWithUnknownAuthError(account_info);
  histogram_tester2.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotThrottleUponAccessTokenFetchingFailure) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const MemberKeysSource&,
              const SecureBoxPublicKey&, AuthenticationFactorType,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  base::HistogramTester histogram_tester;

  // Mimic access token fetching failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError,
           /*key_version=*/0);

  histogram_tester.ExpectUniqueSample(
      /*name=*/"TrustedVault.DeviceRegistrationOutcome." +
          security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationOutcomeForUMA::
          kTransientAccessTokenFetchError,
      /*expected_bucket_count=*/1);

  // Mimic a restart to trigger device registration attempt, which should not be
  // throttled.
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  SetPrimaryAccountWithUnknownAuthError(account_info);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldNotThrottleUponNetworkError) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const MemberKeysSource&,
              const SecureBoxPublicKey&, AuthenticationFactorType,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic network error.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kNetworkError, /*key_version=*/0);

  // Mimic a restart to trigger device registration attempt, which should not be
  // throttled.
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  SetPrimaryAccountWithUnknownAuthError(account_info);
}

// System time can be changed to the past and if this situation not handled,
// requests could be throttled for unreasonable amount of time.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUnthrottleDeviceRegistrationWhenTimeSetToPast) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const MemberKeysSource&,
              const SecureBoxPublicKey&, AuthenticationFactorType,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kOtherError, /*key_version=*/0);

  // Mimic system set to the past.
  clock()->Advance(base::Seconds(-1));

  device_registration_callback =
      TrustedVaultConnection::RegisterAuthenticationFactorCallback();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Reset and set primary account to trigger device registration attempt.
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);
  SetPrimaryAccountWithUnknownAuthError(account_info);

  EXPECT_FALSE(device_registration_callback.is_null());
}

// Unless keys marked as stale, FetchKeys() should be completed immediately,
// without keys download attempt.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchKeysImmediately) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;

  // Make keys downloading theoretically possible.
  StoreKeysAndMimicDeviceRegistration(kVaultKeys, kLastKeyVersion,
                                      account_info);
  SetPrimaryAccountWithUnknownAuthError(account_info);

  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);

  std::vector<std::vector<uint8_t>> fetched_keys;
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/Eq(kVaultKeys)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysWithV1Registration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  SetPrimaryAccountWithUnknownAuthError(account_info);

  ASSERT_THAT(backend()
                  ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                  .device_registered_version(),
              Eq(1));

  const std::vector<uint8_t> kNewVaultKey = {1, 3, 2};
  const int kNewLastKeyVersion = 2;

  std::unique_ptr<SecureBoxKeyPair> device_key_pair;
  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  EXPECT_CALL(*connection(),
              DownloadNewKeys(Eq(account_info),
                              TrustedVaultKeyAndVersionEq(
                                  kInitialVaultKey, kInitialLastKeyVersion),
                              _, _))
      .WillOnce([&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                    std::unique_ptr<SecureBoxKeyPair> key_pair,
                    TrustedVaultConnection::DownloadNewKeysCallback callback) {
        device_key_pair = std::move(key_pair);
        download_keys_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Ensure that the right device key was passed into DownloadNewKeys().
  ASSERT_THAT(device_key_pair, NotNull());
  EXPECT_THAT(device_key_pair->private_key().ExportToBytes(),
              Eq(private_device_key_material));

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed. Note that the client should keep old key as well.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(fetch_keys_callback,
              Run(/*keys=*/ElementsAre(kInitialVaultKey, kNewVaultKey)));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess,
           {kInitialVaultKey, kNewVaultKey}, kNewLastKeyVersion);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatus",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatusV1",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

// The server may clean up some stale keys eventually, client should clean them
// up as well to ensure that the state doesn't diverge. In particular, this may
// cause problems with registering authentication factors, since the server will
// reject request with stale keys.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldCleanUpOldKeysWhenDownloadingNew) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                      kInitialLastKeyVersion, account_info);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  SetPrimaryAccountWithUnknownAuthError(account_info);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys)
      .WillByDefault(
          [&](const CoreAccountInfo&,
              const std::optional<TrustedVaultKeyAndVersion>&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), DownloadNewKeys);
  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  const std::vector<uint8_t> kNewVaultKey = {2, 3, 5};

  // Note that |fetch_keys_callback| should not receive kInitialVaultKey.
  EXPECT_CALL(fetch_keys_callback, Run(ElementsAre(kNewVaultKey)));

  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, {kNewVaultKey},
           kInitialLastKeyVersion + 1);
}

// Regression test for crbug.com/1500258: second FetchKeys() is triggered, while
// first is still ongoing (e.g. keys are being downloaded).
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysAndCompleteConcurrentFetches) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                      kInitialLastKeyVersion, account_info);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  SetPrimaryAccountWithUnknownAuthError(account_info);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys)
      .WillByDefault(
          [&](const CoreAccountInfo&,
              const std::optional<TrustedVaultKeyAndVersion>&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), DownloadNewKeys);
  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback1;
  backend()->FetchKeys(account_info, fetch_keys_callback1.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic second FetchKeys(), note that keys are not downloaded yet and first
  // fetch is not completed.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback2;
  backend()->FetchKeys(account_info, fetch_keys_callback2.Get());

  // Both fetches should be completed once keys are downloaded.
  std::vector<uint8_t> kNewVaultKey = {2, 3, 5};
  EXPECT_CALL(fetch_keys_callback1,
              Run(ElementsAre(kInitialVaultKey, kNewVaultKey)));
  EXPECT_CALL(fetch_keys_callback2,
              Run(ElementsAre(kInitialVaultKey, kNewVaultKey)));

  base::HistogramTester histogram_tester;
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess,
           {kInitialVaultKey, kNewVaultKey}, kInitialLastKeyVersion + 1);

  // Download keys status should be recorded for every fetch.
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatus",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatusV1",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/2);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUntrottleKeysDownloading) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  SetPrimaryAccountWithUnknownAuthError(account_info);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys)
      .WillByDefault(
          [&](const CoreAccountInfo&,
              const std::optional<TrustedVaultKeyAndVersion>&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());
  EXPECT_CALL(*connection(), DownloadNewKeys);

  // FetchKeys() should trigger keys downloading.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  ASSERT_FALSE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  base::HistogramTester histogram_tester;
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kOtherError,
           /*keys=*/std::vector<std::vector<uint8_t>>(),
           /*last_key_version=*/0);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatus",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kOtherError,
      /*expected_bucket_count=*/1);
  EXPECT_TRUE(backend()->AreConnectionRequestsThrottledForTesting());

  download_keys_callback = TrustedVaultConnection::DownloadNewKeysCallback();
  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);
  // Following request should be throttled.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_TRUE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Advance time to pass the throttling duration and trigger another attempt.
  clock()->Advance(StandaloneTrustedVaultBackend::kThrottlingDuration);
  EXPECT_FALSE(backend()->AreConnectionRequestsThrottledForTesting());

  EXPECT_CALL(*connection(), DownloadNewKeys);
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_FALSE(download_keys_callback.is_null());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleIfDownloadingReturnedNoNewKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  SetPrimaryAccountWithUnknownAuthError(account_info);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys)
      .WillByDefault(
          [&](const CoreAccountInfo&,
              const std::optional<TrustedVaultKeyAndVersion>&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), DownloadNewKeys);

  // FetchKeys() should trigger keys downloading.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  ASSERT_FALSE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic the server having no new keys.
  base::HistogramTester histogram_tester;
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kNoNewKeys,
           /*keys=*/std::vector<std::vector<uint8_t>>(),
           /*last_key_version=*/0);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatus",
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys,
      /*expected_bucket_count=*/1);

  EXPECT_TRUE(backend()->AreConnectionRequestsThrottledForTesting());

  // Registration should remain intact.
  EXPECT_TRUE(backend()
                  ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                  .device_registered());
}

// Tests silent device registration (when no vault keys available yet). After
// successful registration, the client should be able to download keys.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldSilentlyRegisterDeviceAndDownloadNewKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kServerConstantKeyVersion = 100;

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys(account_info, _, _))
      .WillOnce([&](const CoreAccountInfo&, const SecureBoxPublicKey&,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kServerConstantKeyVersion);

  // Now the device should be registered.
  trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys(account_info,
                                         TrustedVaultKeyAndVersionEq(
                                             GetConstantTrustedVaultKey(),
                                             kServerConstantKeyVersion),
                                         _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  // FetchKeys() should trigger keys downloading. Note: unlike tests with
  // following regular key rotation, in this case MarkLocalKeysAsStale() isn't
  // called intentionally.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {{1, 2, 3}};
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/kNewVaultKeys));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, kNewVaultKeys,
           /*last_key_version=*/kServerConstantKeyVersion + 1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRedoDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key = StoreKeysAndMimicDeviceRegistration(
      {GetConstantTrustedVaultKey(), kVaultKey}, kLastKeyVersion, account_info);
  // Mimic that device was registered before "redo registration" logic was
  // introduced.
  backend()->SetDeviceRegisteredVersionForTesting(account_info.gaia,
                                                  /*version=*/0);

  // Mimic restart to be able to test histogram recording.
  ResetBackend();

  // Another device registration request should be issued upon setting the
  // primary account.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              std::vector<TrustedVaultKeyAndVersion>{
                  TrustedVaultKeyAndVersion(GetConstantTrustedVaultKey(),
                                            kLastKeyVersion - 1),
                  TrustedVaultKeyAndVersion(kVaultKey, kLastKeyVersion)}),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  {
    base::HistogramTester histogram_tester;
    SetPrimaryAccountWithUnknownAuthError(account_info);
    ASSERT_FALSE(device_registration_callback.is_null());
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistrationState." +
            security_domain_name_for_uma(),
        /*sample=*/
        TrustedVaultDeviceRegistrationStateForUMA::
            kAttemptingRegistrationWithExistingKeyPair,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
        /*sample=*/true,
        /*expected_bucket_count=*/1);

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);

    // Now the device reregistration should be completed.
    trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
        backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
    EXPECT_TRUE(registration_info.device_registered());
    EXPECT_THAT(registration_info.device_registered_version(), Eq(1));
    EXPECT_TRUE(registration_info.has_private_key_material());

    // Ensure device key was reused.
    EXPECT_THAT(ProtoStringToBytes(registration_info.private_key_material()),
                Eq(private_device_key));
    EXPECT_THAT(
        serialized_public_device_key,
        Eq(SecureBoxKeyPair::CreateByPrivateKeyImport(private_device_key)
               ->public_key()
               .ExportToBytes()));
  }
  {
    // Mimic the restart and verify that kAlreadyRegisteredV1 is recorded.
    ResetBackend();

    base::HistogramTester histogram_tester;
    SetPrimaryAccountWithUnknownAuthError(account_info);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistrationState." +
            security_domain_name_for_uma(),
        /*sample=*/
        TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
        /*sample=*/true,
        /*expected_bucket_count=*/1);
  }
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRedoDeviceRegistrationWithConstantKey) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kInitialServerConstantKeyVersion = 100;

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys(account_info, _, _))
      .WillOnce([&](const CoreAccountInfo&, const SecureBoxPublicKey&,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // Pretend that the registration completed successfully.
  ASSERT_FALSE(device_registration_callback.is_null());
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess,
           kInitialServerConstantKeyVersion);
  // Mimic that device was registered before "redo registration" logic was
  // introduced.
  backend()->SetDeviceRegisteredVersionForTesting(account_info.gaia,
                                                  /*version=*/0);

  // Mimic restart to be able to test histogram recording.
  ResetBackend();

  // Another device registration request should be issued upon setting the
  // primary account and it should ignore presence of
  // kInitialServerConstantKeyVersion, e.g. RegisterLocalDeviceWithoutKeys()
  // again.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_redo_registration_callback;
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys(account_info, _, _))
      .WillOnce([&](const CoreAccountInfo&,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_redo_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  {
    const int kNewServerConstantKeyVersion = 101;

    base::HistogramTester histogram_tester;
    SetPrimaryAccountWithUnknownAuthError(account_info);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistrationState." +
            security_domain_name_for_uma(),
        /*sample=*/
        TrustedVaultDeviceRegistrationStateForUMA::
            kAttemptingRegistrationWithExistingKeyPair,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
        /*sample=*/true,
        /*expected_bucket_count=*/1);

    // Pretend that the registration completed successfully and that constant
    // key version has changed between device registration requests.
    ASSERT_FALSE(device_redo_registration_callback.is_null());
    std::move(device_redo_registration_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess,
             kNewServerConstantKeyVersion);

    // Now the device reregistration should be completed.
    trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
        backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
    EXPECT_TRUE(registration_info.device_registered());
    EXPECT_THAT(registration_info.device_registered_version(), Eq(1));
    EXPECT_TRUE(registration_info.has_private_key_material());

    // Read the file from disk and verify that kNewServerConstantKeyVersion is
    // stored.
    trusted_vault_pb::LocalTrustedVault proto =
        ReadLocalTrustedVaultFile(file_path());
    ASSERT_THAT(proto.user_size(), Eq(1));
    EXPECT_THAT(proto.user(0).last_vault_key_version(),
                Eq(kNewServerConstantKeyVersion));
  }
  {
    // Mimic the restart and verify that kAlreadyRegisteredV1 is recorded.
    ResetBackend();

    base::HistogramTester histogram_tester;
    SetPrimaryAccountWithUnknownAuthError(account_info);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistrationState." +
            security_domain_name_for_uma(),
        /*sample=*/
        TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
        /*sample=*/true,
        /*expected_bucket_count=*/1);
  }
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRegisterWithRecentVersionAndNotRedoRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key = StoreKeysAndMimicDeviceRegistration(
      {kVaultKey}, kLastKeyVersion, account_info);
  EXPECT_THAT(backend()
                  ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                  .device_registered_version(),
              Eq(1));

  // Mimic restart to be able to test histogram recording.
  ResetBackend();

  // No registration attempt should be made, since device is already registered
  // with version 1.
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(account_info);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistrationState." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DeviceRegistered." + security_domain_name_for_uma(),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldAddTrustedRecoveryMethod) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2}, {1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  SetPrimaryAccountWithUnknownAuthError(account_info);
  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorType(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::MockCallback<base::OnceClosure> completion_callback;
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());

  // The operation should be in flight.
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldIgnoreTrustedRecoveryMethodWithInvalidPublicKey) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 0;
  const std::vector<uint8_t> kInvalidPublicKey = {1, 2, 3, 4};
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  ASSERT_THAT(SecureBoxPublicKey::CreateByImport(kInvalidPublicKey), IsNull());

  SetPrimaryAccountWithUnknownAuthError(account_info);
  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);

  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(completion_callback, Run());
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kInvalidPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferTrustedRecoveryMethodUntilPrimaryAccount) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);
  ASSERT_FALSE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // No request should be issued while there is no primary account.
  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());
  EXPECT_TRUE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // Upon setting a primary account, RegisterAuthenticationFactor() should be
  // invoked. It should in fact be called twice: one for device registration,
  // and one for the AddTrustedRecoveryMethod() call being tested here.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          _, _, _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _));
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorType(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        // Note: TrustedVaultConnection::Request doesn't support
        // cancellation, so these tests don't cover the contract that
        // caller should store Request object until it's completed or need
        // to be cancelled.
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // The operation should be in flight.
  EXPECT_FALSE(backend()->HasPendingTrustedRecoveryMethodForTesting());
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferTrustedRecoveryMethodUntilPersistentAuthErrorFixed) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  // Mimic device previously registered with some keys.
  StoreKeysAndMimicDeviceRegistration(kVaultKeys, kLastKeyVersion,
                                      account_info);

  // Mimic entering a persistent auth error.
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kPersistentAuthError);

  // No request should be issued while there is a persistent auth error.
  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());

  EXPECT_TRUE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // Upon resolving the auth error, the request should be issued.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorType(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        // Note: TrustedVaultConnection::Request doesn't support
        // cancellation, so these tests don't cover the contract that
        // caller should store Request object until it's completed or need
        // to be cancelled.
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kNoPersistentAuthErrors);

  // The operation should be in flight.
  EXPECT_FALSE(backend()->HasPendingTrustedRecoveryMethodForTesting());
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
}

// Verifies that Backend can process device registration and keys downloading
// concurrently, when device registration is going to succeed and triggered
// first (to ensure that keys downloading doesn't cancel device registration).
// This is not a likely scenario (keys downloading attempt is an indicator that
// device registration will fail), but Backend shouldn't work under this
// assumption as already reflected on the data level:
// |keys_marked_as_stale_by_consumer| doesn't imply
// |last_registration_returned_local_data_obsolete|.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRegisterDeviceWhileConcurrentlyDownloadingKeys) {
  // Prepare state where both requests are meaningful:
  // 1. This is "redo device registration" attempt (otherwise FetchKeys() will
  // fail early).
  // 2. Local keys are marked as stale (otherwise FetchKeys() will succeed
  // early).
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;

  StoreKeysAndMimicDeviceRegistration(kTrustedVaultKeys, kLastKeyVersion,
                                      account_info);
  // Mimic that device was registered before "redo registration" logic was
  // introduced.
  backend()->SetDeviceRegisteredVersionForTesting(account_info.gaia,
                                                  /*version=*/0);
  backend()->MarkLocalKeysAsStale(account_info);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      redo_device_registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(GetTrustedVaultKeysWithVersions(
              {kTrustedVaultKeys}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        redo_device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  // Trigger "redo device registration".
  SetPrimaryAccountWithUnknownAuthError(account_info);

  // Trigger keys downloading, ensure that FetchKeys() actually starts
  // downloading attempt (e.g. keys are not fetched immediately).
  EXPECT_CALL(*connection(),
              DownloadNewKeys(Eq(account_info),
                              TrustedVaultKeyAndVersionEq(kTrustedVaultKeys[0],
                                                          kLastKeyVersion),
                              /*device_key_pair=*/NotNull(), _))
      .WillOnce(
          Return(ByMove(std::make_unique<TrustedVaultConnection::Request>())));
  backend()->FetchKeys(account_info, base::DoNothing());

  {
    trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
        backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
    ASSERT_THAT(registration_info.device_registered_version(), Ne(1));
  }
  // Complete "redo device registration" and verify it succeeds.
  ASSERT_FALSE(redo_device_registration_callback.is_null());
  std::move(redo_device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
  trusted_vault_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_THAT(registration_info.device_registered_version(), Eq(1));
}

// Verifies that Backend can process device registration and keys downloading
// concurrently, when keys downloading is going to succeed and triggered first
// (to ensure that device registration doesn't cancel keys downloading).
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysWhileConcurrentlyRegisteringDevice) {
  // Prepare state where both requests are meaningful:
  // 1. This is "redo device registration" attempt (otherwise FetchKeys() will
  // fail early).
  // 2. Local keys are marked as stale (otherwise FetchKeys() will succeed
  // early).
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialTrustedVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  StoreKeysAndMimicDeviceRegistration({kInitialTrustedVaultKey},
                                      kInitialLastKeyVersion, account_info);
  // Note: SetPrimaryAccount() doesn't trigger device registration yet (not
  // needed), the test exploits |has_persistent_auth_error| to trigger it by
  // another SetPrimaryAccount() later.
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kPersistentAuthError);
  // Mimic that device was registered before "redo registration" logic was
  // introduced.
  backend()->SetDeviceRegisteredVersionForTesting(account_info.gaia,
                                                  /*version=*/0);
  backend()->MarkLocalKeysAsStale(account_info);

  // Trigger keys downloading, ensure that FetchKeys() actually starts
  // downloading attempt (e.g. keys are not fetched immediately).
  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(*connection(), DownloadNewKeys(Eq(account_info),
                                             TrustedVaultKeyAndVersionEq(
                                                 kInitialTrustedVaultKey,
                                                 kInitialLastKeyVersion),
                                             /*device_key_pair=*/NotNull(), _))
      .WillOnce([&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                    std::unique_ptr<SecureBoxKeyPair> key_pair,
                    TrustedVaultConnection::DownloadNewKeysCallback callback) {
        download_keys_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Note: RegisterAuthenticationFactor() will be actually called two times,
  // once upon SetPrimaryAccount() with stale keys and once upon keys
  // downloading with new keys.
  EXPECT_CALL(*connection(),
              RegisterAuthenticationFactor(
                  Eq(account_info), _, _,
                  Eq(AuthenticationFactorType(LocalPhysicalDevice())), _))
      .WillRepeatedly([&]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  // Trigger "redo device registration".
  backend()->SetPrimaryAccount(
      account_info, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kNoPersistentAuthErrors);

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<uint8_t> kNewTrustedVaultKey = {2, 3, 4};
  EXPECT_CALL(
      fetch_keys_callback,
      Run(/*keys*/ ElementsAre(kInitialTrustedVaultKey, kNewTrustedVaultKey)));
  ASSERT_FALSE(download_keys_callback.is_null());
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess,
           {kInitialTrustedVaultKey, kNewTrustedVaultKey},
           kInitialLastKeyVersion + 1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRegisterRecoveryKeyAndUploadToKeyStore) {
  base::test::SingleThreadTaskEnvironment environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const std::vector<uint8_t> kRecoveryPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  SetUpRecoveryKey(kRecoveryPublicKey);
  ResetBackend();

  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;
  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  SetPrimaryAccountWithUnknownAuthError(account_info);

  base::RunLoop register_auth_factor_run_loop;
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  std::vector<uint8_t> registered_public_key_bytes;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _, Eq(AuthenticationFactorType(LockScreenKnowledgeFactor())), _))
      .WillOnce([&](const CoreAccountInfo&,
                    const MemberKeysSource& member_keys_source,
                    const SecureBoxPublicKey& auth_factor_public_key,
                    AuthenticationFactorType,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        register_auth_factor_run_loop.Quit();
        registered_public_key_bytes = auth_factor_public_key.ExportToBytes();
        registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::RunLoop update_recovery_key_store_run_loop;
  RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback
      update_recovery_key_store_cb;
  EXPECT_CALL(*recovery_key_store_connection(),
              UpdateRecoveryKeyStore(account_info, _, _))
      .WillOnce([&](const CoreAccountInfo& account_info,
                    const trusted_vault_pb::Vault& request,
                    RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback
                        callback) {
        update_recovery_key_store_run_loop.Quit();
        update_recovery_key_store_cb = std::move(callback);
        return std::make_unique<RecoveryKeyStoreConnection::Request>();
      });

  backend()->SetRecoveryKeyStoreUploadEnabled(account_info, true);

  // The uploaded recovery key should be registered as an authentication factor
  // with the security domain.
  register_auth_factor_run_loop.Run();
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
  EXPECT_THAT(registered_public_key_bytes, Eq(kRecoveryPublicKey));

  // Wait for recovery key store upload to complete.
  update_recovery_key_store_run_loop.Run();
  std::move(update_recovery_key_store_cb)
      .Run(UpdateRecoveryKeyStoreStatus::kSuccess);

  const trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(1));
  EXPECT_TRUE(proto.user(0)
                  .recovery_key_store_state()
                  .recovery_key_is_registered_to_security_domain());
  EXPECT_THAT(
      ProtoStringToBytes(proto.user(0).recovery_key_store_state().public_key()),
      Eq(kRecoveryPublicKey));
}

}  // namespace

}  // namespace trusted_vault
