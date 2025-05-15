// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_backend.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/test/fake_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::ByMove;
using testing::ElementsAre;
using testing::Eq;
using testing::Ge;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::Ne;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;

MATCHER_P(MatchTrustedVaultKeyAndVersions, expected, "") {
  const auto* trusted_vault_keys =
      std::get_if<std::vector<TrustedVaultKeyAndVersion>>(&arg);
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

CoreAccountInfo MakeAccountInfoWithGaiaId(const std::string& gaia_id) {
  CoreAccountInfo account_info;
  account_info.gaia = GaiaId(gaia_id);
  return account_info;
}

class MockDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void, NotifyRecoverabilityDegradedChanged, (), (override));
};

class FakeLocalRecoveryFactor : public LocalRecoveryFactor {
 public:
  FakeLocalRecoveryFactor(StandaloneTrustedVaultStorage* storage,
                          TrustedVaultThrottlingConnection* connection,
                          CoreAccountInfo account)
      : storage_(storage), connection_(connection), account_(account) {}
  FakeLocalRecoveryFactor(const FakeLocalRecoveryFactor&) = delete;
  FakeLocalRecoveryFactor& operator=(const FakeLocalRecoveryFactor&) = delete;
  ~FakeLocalRecoveryFactor() override = default;

  LocalRecoveryFactorType GetRecoveryFactorType() const override {
    return LocalRecoveryFactorType::kPhysicalDevice;
  }

  void AttemptRecovery(AttemptRecoveryCallback callback) override {
    CHECK(connection_);
    CHECK(recovery_callback_.is_null());
    attempt_recovery_was_called_ = true;

    if (!is_registered_) {
      std::move(callback).Run(RecoveryStatus::kFailure,
                              /*new_vault_keys=*/{},
                              /*last_vault_key_version=*/0);
      return;
    }
    if (connection_->AreRequestsThrottled(account_)) {
      std::move(callback).Run(RecoveryStatus::kFailure,
                              /*new_vault_keys=*/{},
                              /*last_vault_key_version=*/0);
      return;
    }

    recovery_callback_ = std::move(callback);
  }

  bool IsRegistered() override { return is_registered_; }

  void MarkAsNotRegistered() override { is_registered_ = false; }

  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      RegisterCallback callback) override {
    CHECK(connection_);
    CHECK(register_callback_.is_null());
    maybe_register_was_called_ = true;

    auto* per_user_vault = storage_->FindUserVault(account_.gaia);
    CHECK(per_user_vault);

    if (is_registered_) {
      return TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAlreadyRegisteredV1;
    }
    if (per_user_vault->last_registration_returned_local_data_obsolete()) {
      return TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kLocalKeysAreStale;
    }
    if (connection_->AreRequestsThrottled(account_)) {
      return TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kThrottledClientSide;
    }

    register_callback_ = base::BindOnce(
        base::BindLambdaForTesting([this, per_user_vault](
                                       RegisterCallback cb,
                                       TrustedVaultRegistrationStatus status,
                                       int key_version, bool had_local_keys) {
          if (status == TrustedVaultRegistrationStatus::kSuccess ||
              status == TrustedVaultRegistrationStatus::kAlreadyRegistered) {
            is_registered_ = true;
          }
          if (status == TrustedVaultRegistrationStatus::kLocalDataObsolete) {
            per_user_vault->set_last_registration_returned_local_data_obsolete(
                true);
            storage_->WriteDataToDisk();
          }
          std::move(cb).Run(status, key_version, had_local_keys);
        }),
        std::move(callback));

    if (key_pair_exists_) {
      return TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair;
    } else {
      key_pair_exists_ = true;
      return TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair;
    }
  }

  bool AttemptRecoveryWasCalled() const { return attempt_recovery_was_called_; }

  bool MaybeRegisterWasCalled() const { return maybe_register_was_called_; }

  void ExpectAttemptRecoveryAndRunCallback(
      RecoveryStatus status,
      const std::vector<std::vector<uint8_t>>& new_vault_keys,
      int last_vault_key_version) {
    ASSERT_FALSE(recovery_callback_.is_null());
    std::move(recovery_callback_)
        .Run(status, new_vault_keys, last_vault_key_version);
  }

  void ExpectMaybeRegisterAndRunCallback(TrustedVaultRegistrationStatus status,
                                         int key_version,
                                         bool had_local_keys) {
    ASSERT_FALSE(register_callback_.is_null());
    std::move(register_callback_).Run(status, key_version, had_local_keys);
  }

  void SetStorage(StandaloneTrustedVaultStorage* storage) {
    storage_ = storage;
  }

  void SetConnection(TrustedVaultThrottlingConnection* new_connection) {
    connection_ = new_connection;
  }

  void ResetCallInfo() {
    recovery_callback_.Reset();
    register_callback_.Reset();
    attempt_recovery_was_called_ = false;
    maybe_register_was_called_ = false;
  }

 private:
  raw_ptr<StandaloneTrustedVaultStorage> storage_;
  raw_ptr<TrustedVaultThrottlingConnection> connection_;
  const CoreAccountInfo account_;

  bool is_registered_ = false;
  bool key_pair_exists_ = false;
  bool attempt_recovery_was_called_ = false;
  bool maybe_register_was_called_ = false;
  AttemptRecoveryCallback recovery_callback_;
  RegisterCallback register_callback_;
};

// The LocalRecoveryFactor created by TestLocalRecoveryFactorsFactory below is
// destroyed every time the primary account of StandaloneTrustedVaultBackend is
// changed. However, for testing, we want to keep state in
// FakeLocalRecoveryFactor across those events. Thus, this class is introduced -
// it can repeatedly be re-created while still pointing to the same
// FakeLocalRecoveryFactor instance.
class ForwardingLocalRecoveryFactor : public LocalRecoveryFactor {
 public:
  explicit ForwardingLocalRecoveryFactor(raw_ptr<LocalRecoveryFactor> delegate)
      : delegate_(delegate) {
    CHECK(delegate_);
  }
  ForwardingLocalRecoveryFactor(const ForwardingLocalRecoveryFactor&) = delete;
  ForwardingLocalRecoveryFactor& operator=(
      const ForwardingLocalRecoveryFactor&) = delete;
  ~ForwardingLocalRecoveryFactor() override = default;

  LocalRecoveryFactorType GetRecoveryFactorType() const override {
    return delegate_->GetRecoveryFactorType();
  }
  void AttemptRecovery(AttemptRecoveryCallback callback) override {
    delegate_->AttemptRecovery(std::move(callback));
  }
  bool IsRegistered() override { return delegate_->IsRegistered(); }
  void MarkAsNotRegistered() override { delegate_->MarkAsNotRegistered(); }
  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      RegisterCallback callback) override {
    return delegate_->MaybeRegister(std::move(callback));
  }

 private:
  raw_ptr<LocalRecoveryFactor> delegate_;
};

class TestLocalRecoveryFactorsFactory
    : public StandaloneTrustedVaultBackend::LocalRecoveryFactorsFactory {
 public:
  explicit TestLocalRecoveryFactorsFactory(size_t num_local_recovery_factors)
      : num_local_recovery_factors_(num_local_recovery_factors) {
    CHECK(num_local_recovery_factors_ > 0);
  }
  TestLocalRecoveryFactorsFactory(const TestLocalRecoveryFactorsFactory&) =
      delete;
  TestLocalRecoveryFactorsFactory& operator=(
      const TestLocalRecoveryFactorsFactory&) = delete;
  ~TestLocalRecoveryFactorsFactory() override = default;

  std::vector<std::unique_ptr<LocalRecoveryFactor>> CreateLocalRecoveryFactors(
      SecurityDomainId security_domain_id,
      StandaloneTrustedVaultStorage* storage,
      TrustedVaultThrottlingConnection* connection,
      const CoreAccountInfo& account) override {
    std::vector<FakeLocalRecoveryFactor*> fake_recovery_factors =
        GetOrCreateRecoveryFactors(storage, connection, account);

    std::vector<std::unique_ptr<LocalRecoveryFactor>> local_recovery_factors;
    for (auto* fake_recovery_factor : fake_recovery_factors) {
      fake_recovery_factor->ResetCallInfo();
      local_recovery_factors.emplace_back(
          std::make_unique<ForwardingLocalRecoveryFactor>(
              fake_recovery_factor));
    }
    return local_recovery_factors;
  }

  std::map<std::optional<GaiaId>,
           std::vector<std::unique_ptr<FakeLocalRecoveryFactor>>>
  GetRecoveryFactors() {
    return std::move(recovery_factors_);
  }

  void SetRecoveryFactors(
      StandaloneTrustedVaultStorage* new_storage,
      TrustedVaultThrottlingConnection* new_connection,
      std::map<std::optional<GaiaId>,
               std::vector<std::unique_ptr<FakeLocalRecoveryFactor>>>&&
          recovery_factors) {
    recovery_factors_ = std::move(recovery_factors);
    // Storage and the connection might have changed, make sure to update all
    // fake recovery factors to point to the new one.
    // Note: FakeLocalRecoveryFactor does not store any recovery factor related
    // state in storage, but needs it to access per user information.
    for (auto& user_recovery_factors : recovery_factors_) {
      for (auto& recovery_factor : user_recovery_factors.second) {
        recovery_factor->SetStorage(new_storage);
        recovery_factor->SetConnection(new_connection);
      }
    }
  }

  std::vector<FakeLocalRecoveryFactor*> GetOrCreateRecoveryFactors(
      StandaloneTrustedVaultStorage* storage,
      TrustedVaultThrottlingConnection* connection,
      const CoreAccountInfo& account) {
    if (!recovery_factors_.contains(account.gaia)) {
      std::vector<std::unique_ptr<FakeLocalRecoveryFactor>> recovery_factors;
      for (size_t i = 0; i < num_local_recovery_factors_; ++i) {
        recovery_factors.emplace_back(std::make_unique<FakeLocalRecoveryFactor>(
            storage, connection, account));
      }
      recovery_factors_.emplace(account.gaia, std::move(recovery_factors));
    }

    std::vector<FakeLocalRecoveryFactor*> ret;
    for (const auto& it : recovery_factors_[account.gaia]) {
      ret.push_back(it.get());
    }
    return ret;
  }

 private:
  const size_t num_local_recovery_factors_;
  std::map<std::optional<GaiaId>,
           std::vector<std::unique_ptr<FakeLocalRecoveryFactor>>>
      recovery_factors_;
};

class StandaloneTrustedVaultBackendTest : public testing::Test {
 public:
  StandaloneTrustedVaultBackendTest() { ResetBackend(); }

  ~StandaloneTrustedVaultBackendTest() override = default;

  void ResetBackend() {
    ResetBackend(std::make_unique<
                 testing::NiceMock<MockTrustedVaultThrottlingConnection>>());
  }

  void ResetBackend(
      std::unique_ptr<testing::NiceMock<MockTrustedVaultThrottlingConnection>>
          connection) {
    auto file_access = std::make_unique<FakeFileAccess>();
    if (file_access_) {
      // We only want to reset the backend, not the underlying faked file.
      file_access->SetStoredLocalTrustedVault(
          file_access_->GetStoredLocalTrustedVault());
    }
    file_access_ = file_access.get();
    auto storage =
        StandaloneTrustedVaultStorage::CreateForTesting(std::move(file_access));

    auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();

    auto local_recovery_factors_factory =
        std::make_unique<TestLocalRecoveryFactorsFactory>(
            num_local_recovery_factors_);
    if (local_recovery_factors_factory_) {
      // We only want to reset the backend, not the underlying faked recovery
      // factors incl. their state.
      local_recovery_factors_factory->SetRecoveryFactors(
          storage.get(), connection.get(),
          local_recovery_factors_factory_->GetRecoveryFactors());
    }
    local_recovery_factors_factory_ = local_recovery_factors_factory.get();

    storage_ = storage.get();
    connection_ = connection.get();

    backend_ = StandaloneTrustedVaultBackend::CreateForTesting(
        security_domain_id(), std::move(storage), std::move(delegate),
        std::move(connection), std::move(local_recovery_factors_factory));
    backend_->ReadDataFromDisk();
  }

  // Sets the number of local recovery factors to create during tests.
  // Note: This only takes effect when ResetBackend() is called.
  void SetNumLocalRecoveryFactors(size_t num_local_recovery_factors) {
    num_local_recovery_factors_ = num_local_recovery_factors;
  }

  StandaloneTrustedVaultStorage* storage() { return storage_; }

  FakeFileAccess* file_access() { return file_access_; }

  MockTrustedVaultThrottlingConnection* connection() { return connection_; }

  std::vector<FakeLocalRecoveryFactor*> GetOrCreateRecoveryFactors(
      const CoreAccountInfo& account) {
    return local_recovery_factors_factory_->GetOrCreateRecoveryFactors(
        storage_, connection_, account);
  }

  // Shorthand to get/create the first recovery factor.
  FakeLocalRecoveryFactor* GetOrCreateRecoveryFactor(
      const CoreAccountInfo& account) {
    return GetOrCreateRecoveryFactors(account)[0];
  }

  std::string GetRecoveryFactorTypeForUMA(
      FakeLocalRecoveryFactor* recovery_factor) {
    return GetLocalRecoveryFactorNameForUma(
        recovery_factor->GetRecoveryFactorType());
  }

  StandaloneTrustedVaultBackend* backend() { return backend_.get(); }

  SecurityDomainId security_domain_id() const {
    return SecurityDomainId::kChromeSync;
  }

  std::string security_domain_name_for_uma() const {
    return GetSecurityDomainNameForUma(security_domain_id());
  }

  void SetPrimaryAccountWithUnknownAuthError(
      std::optional<CoreAccountInfo> primary_account) {
    backend_->SetPrimaryAccount(
        primary_account,
        StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown);
  }

  // Stores |vault_keys| and mimics successful recovery factor registration
  // (using the FakeRecoveryFactor).
  void StoreKeysAndMimicRecoveryFactorRegistration(
      const std::vector<std::vector<uint8_t>>& vault_keys,
      int last_vault_key_version,
      CoreAccountInfo account_info) {
    DCHECK(!vault_keys.empty());
    backend_->StoreKeys(account_info.gaia, vault_keys, last_vault_key_version);

    // Setting the primary account will trigger recovery factor registration.
    SetPrimaryAccountWithUnknownAuthError(account_info);

    // Pretend that the registration completed successfully.
    for (auto* recovery_factor : GetOrCreateRecoveryFactors(account_info)) {
      recovery_factor->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kSuccess, last_vault_key_version,
          true);
    }

    // Reset primary account.
    SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);
  }

 private:
  size_t num_local_recovery_factors_ = 1;
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;
  raw_ptr<StandaloneTrustedVaultStorage> storage_ = nullptr;
  raw_ptr<FakeFileAccess> file_access_ = nullptr;
  raw_ptr<testing::NiceMock<MockTrustedVaultThrottlingConnection>> connection_ =
      nullptr;
  raw_ptr<TestLocalRecoveryFactorsFactory> local_recovery_factors_factory_;
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
      file_access()->GetStoredLocalTrustedVault();
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

  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);
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
  backend()->GetIsRecoverabilityDegraded(kAccountInfo, cb.Get());
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
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user2");
  // This GetIsRecoverabilityDegraded() is corresponding to a late
  // SetPrimaryAccount(), in this case the callback should be deferred and
  // invoked when SetPrimaryAccount() is called.
  backend()->GetIsRecoverabilityDegraded(kAccountInfo, cb.Get());

  Mock::VerifyAndClearExpectations(&cb);

  ON_CALL(*connection(), DownloadIsRecoverabilityDegraded(Eq(kAccountInfo), _))
      .WillByDefault([](const CoreAccountInfo&,
                        TrustedVaultConnection::IsRecoverabilityDegradedCallback
                            callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // The callback should be invoked on SetPrimaryAccount() since the last
  // GetIsRecoverabilityDegraded() was called with the same account.
  EXPECT_CALL(cb, Run(true));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);
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
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldReadAndFetchNonEmptyKeys) {
  const CoreAccountInfo kAccountInfo1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo kAccountInfo2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(kAccountInfo1.gaia.ToString());
  user_data2->set_gaia_id(kAccountInfo2.gaia.ToString());
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data2->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());

  file_access()->SetStoredLocalTrustedVault(initial_data);
  backend()->ReadDataFromDisk();

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  backend()->FetchKeys(kAccountInfo1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  backend()->FetchKeys(kAccountInfo2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFilterOutConstantKey) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data =
      initial_data.add_user();
  user_data->set_gaia_id(kAccountInfo.gaia.ToString());
  user_data->add_vault_key()->set_key_material(
      GetConstantTrustedVaultKey().data(), GetConstantTrustedVaultKey().size());
  user_data->add_vault_key()->set_key_material(kKey.data(), kKey.size());

  file_access()->SetStoredLocalTrustedVault(initial_data);
  backend()->ReadDataFromDisk();

  // Keys should be fetched immediately, constant key must be filtered out.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldStoreKeys) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  backend()->StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/7);
  backend()->StoreKeys(kGaiaId2, {kKey2}, /*last_key_version=*/8);
  // Keys for |kGaiaId2| overridden, so |kKey2| should be lost.
  backend()->StoreKeys(kGaiaId2, {kKey3, kKey4}, /*last_key_version=*/9);

  // Read the content from storage.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(proto.user_size(), Eq(2));
  EXPECT_THAT(proto.user(0).vault_key(), ElementsAre(KeyMaterialEq(kKey1)));
  EXPECT_THAT(proto.user(0).last_vault_key_version(), Eq(7));
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(kKey3), KeyMaterialEq(kKey4)));
  EXPECT_THAT(proto.user(1).last_vault_key_version(), Eq(9));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchPreviouslyStoredKeys) {
  const CoreAccountInfo kAccountInfo1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo kAccountInfo2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(kAccountInfo1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(kAccountInfo2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Reset the backend, which makes it re-read the data stored above.
  ResetBackend();

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  backend()->FetchKeys(kAccountInfo1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  backend()->FetchKeys(kAccountInfo2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldFetchPreviouslyStoredKeysWithNullConnection) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");

  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};

  backend()->StoreKeys(kAccountInfo.gaia, {kKey}, /*last_key_version=*/0);

  // Reset the backend without a connection, which makes it re-read the data
  // stored above.
  ResetBackend(/*connection=*/nullptr);

  // Keys should be fetched immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDeleteNonPrimaryAccountKeys) {
  const CoreAccountInfo kAccountInfo1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo kAccountInfo2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(kAccountInfo1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(kAccountInfo2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Make sure that backend handles primary account changes prior
  // UpdateAccountsInCookieJarInfo() call.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo1);
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);

  // Keys should be removed immediately if account is not primary and not in
  // cookie jar.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());

  // Keys should be removed from both in-memory and disk storages.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(kAccountInfo1, fetch_keys_callback.Get());

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(kAccountInfo2, fetch_keys_callback.Get());

  // Read the file from storage and verify that keys were removed.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferPrimaryAccountKeysDeletion) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(kAccountInfo.gaia, {kKey}, /*last_key_version=*/0);
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Reset primary account, keys should be deleted from both in-memory and disk
  // storage.
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Read the file from storage and verify that keys were removed.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldCompletePrimaryAccountKeysDeletionAfterRestart) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(kAccountInfo.gaia, {kKey}, /*last_key_version=*/0);
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Mimic browser restart and reset primary account. Don't use the default
  // connection, otherwise FetchKeys() below would perform a recovery factor
  // registration.
  ResetBackend(/*connection=*/nullptr);
  SetPrimaryAccountWithUnknownAuthError(/*primary_account=*/std::nullopt);

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->SetPrimaryAccount(
      kAccountInfo,
      StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown);
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Read the file from storage and verify that keys were removed.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRegisterRecoveryFactors) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  // Setting the primary account will trigger recovery factor registration.
  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistered." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/false,
      /*expected_bucket_count=*/1);

  // Pretend that the registration completed successfully.
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion, true);
  EXPECT_TRUE(GetOrCreateRecoveryFactor(kAccountInfo)->IsRegistered());
  histogram_tester.ExpectUniqueSample(
      /*name=*/"TrustedVault.RecoveryFactorRegistrationOutcome." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldClearDataAndAttemptRecoveryFactorRegistration) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kInitialVaultKeys = {{1, 2, 3}};
  const int kInitialLastKeyVersion = 1;

  // Mimic fake recovery factor previously registered with some keys.
  StoreKeysAndMimicRecoveryFactorRegistration(
      kInitialVaultKeys, kInitialLastKeyVersion, kAccountInfo);

  // Set primary account to trigger immediate recovery factor registration
  // attempt upon reset.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Clear data for |kAccountInfo|, keys should be removed and registration
  // attempt should be triggered.
  // TODO(crbug.com/405381481): Note that the fake recovery factor isn't reset
  // by ClearLocalDataForAccount() because it doesn't store its state in the
  // shared storage. Thus, it's reset explicitly here.
  GetOrCreateRecoveryFactor(kAccountInfo)->MarkAsNotRegistered();
  backend()->ClearLocalDataForAccount(kAccountInfo);
  // Let the registration attempt fail, so the recovery attempt triggered below
  // returns with "not registered" immediately.
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kLocalDataObsolete,
          kInitialLastKeyVersion + 1, false);

  GetOrCreateRecoveryFactor(kAccountInfo)->ResetCallInfo();
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  // Expect a recovery which fails because the fake recovery factor isn't
  // registered. This doesn't trigger a new registration attempt.
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  EXPECT_TRUE(
      GetOrCreateRecoveryFactor(kAccountInfo)->AttemptRecoveryWasCalled());
  EXPECT_FALSE(
      GetOrCreateRecoveryFactor(kAccountInfo)->MaybeRegisterWasCalled());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRetryRecoveryFactorRegistrationWhenAuthErrorResolved) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(
      kAccountInfo, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kPersistentAuthError);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError,
          kLastKeyVersion, true);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);

  // When the auth error is resolved, the registration should be retried.
  base::HistogramTester histogram_tester2;
  backend()->SetPrimaryAccount(
      kAccountInfo, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kNoPersistentAuthErrors);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion, true);

  // The second attempt should NOT have logged the histogram, following the
  // histogram's definition that it should be logged once.
  histogram_tester2.ExpectTotalCount(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*expected_count=*/0);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldTryToRegisterRecoverFactorsEvenIfLocalKeysAreStale) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  EXPECT_TRUE(
      GetOrCreateRecoveryFactor(kAccountInfo)->MaybeRegisterWasCalled());
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRecordLocalKeysAreStale) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  auto* per_user_vault = storage()->FindUserVault(kAccountInfo.gaia);
  per_user_vault->set_last_registration_returned_local_data_obsolete(true);
  storage()->WriteDataToDisk();

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kLocalKeysAreStale,
      /*expected_bucket_count=*/1);
}

TEST_F(
    StandaloneTrustedVaultBackendTest,
    ShouldRegisterRecoveryFactorsAlthoughPreviousAttemptFailedUponNewStoredKeys) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialKeys = {1, 2, 3};
  const int kInitialKeysVersion = 5;
  const std::vector<uint8_t> kNewKeys = {1, 2, 3, 4};
  const int kNewKeysVersion = 6;

  backend()->StoreKeys(kAccountInfo.gaia, {kInitialKeys}, kInitialKeysVersion);

  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kLocalDataObsolete,
          kInitialKeysVersion, true);

  // StoreKeys() should trigger a registration nevertheless.
  backend()->StoreKeys(kAccountInfo.gaia, {kNewKeys}, kNewKeysVersion);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kSuccess, kNewKeysVersion, true);
  EXPECT_TRUE(GetOrCreateRecoveryFactor(kAccountInfo)->IsRegistered());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleOnFailedRecoveryFactorRegistration) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  // Setting the primary account will trigger recovery factor registration.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Mimic transient failure.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kOtherError, 0, true);
  Mock::VerifyAndClearExpectations(connection());

  // Mimic a restart to trigger recovery factor registration attempt.
  base::HistogramTester histogram_tester;
  ResetBackend();
  EXPECT_CALL(*connection(), AreRequestsThrottled).WillOnce(Return(true));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);
  EXPECT_TRUE(
      GetOrCreateRecoveryFactor(kAccountInfo)->MaybeRegisterWasCalled());
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kThrottledClientSide,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotThrottleUponAccessTokenFetchingFailure) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  // Setting the primary account will trigger recovery factor registration.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  base::HistogramTester histogram_tester;
  // Mimic access token fetching failure. The expectation is that the backend
  // doesn't treat this as a failure for throttling.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling).Times(0);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError, 0,
          true);

  histogram_tester.ExpectUniqueSample(
      /*name=*/"TrustedVault.RecoveryFactorRegistrationOutcome." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kTransientAccessTokenFetchError,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldNotThrottleUponNetworkError) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  // Setting the primary account will trigger recovery factor registration.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Mimic network error. This should not throttle.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling).Times(0);
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kNetworkError, 0, true);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRegisterAllLocalRecoveryFactors) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  SetNumLocalRecoveryFactors(2);
  ResetBackend();

  backend()->StoreKeys(kAccountInfo.gaia, {kVaultKey}, kLastKeyVersion);

  // Setting the primary account will trigger recovery factor registration.
  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  std::vector<FakeLocalRecoveryFactor*> recovery_factors =
      GetOrCreateRecoveryFactors(kAccountInfo);
  ASSERT_THAT(recovery_factors, SizeIs(2));

  // Pretend that the registration completed successfully for the first factor.
  recovery_factors[0]->ExpectMaybeRegisterAndRunCallback(
      TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion, true);
  EXPECT_TRUE(recovery_factors[0]->IsRegistered());
  histogram_tester.ExpectBucketCount(
      /*name=*/"TrustedVault.RecoveryFactorRegistrationOutcome." +
          GetRecoveryFactorTypeForUMA(recovery_factors[0]) + "." +
          security_domain_name_for_uma(),
      /*sample=*/TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kSuccess,
      /*expected_count=*/1);

  // Pretend that the registration failed for the second factor.
  recovery_factors[1]->ExpectMaybeRegisterAndRunCallback(
      TrustedVaultRegistrationStatus::kNetworkError, kLastKeyVersion, true);
  EXPECT_FALSE(recovery_factors[1]->IsRegistered());
  histogram_tester.ExpectBucketCount(
      /*name=*/"TrustedVault.RecoveryFactorRegistrationOutcome." +
          GetRecoveryFactorTypeForUMA(recovery_factors[1]) + "." +
          security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kNetworkError,
      /*expected_count=*/1);
}

// Unless keys marked as stale, FetchKeys() should be completed immediately,
// without keys download attempt.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchKeysImmediately) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;

  // Make keys downloading theoretically possible.
  StoreKeysAndMimicRecoveryFactorRegistration(kVaultKeys, kLastKeyVersion,
                                              kAccountInfo);
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);

  std::vector<std::vector<uint8_t>> fetched_keys;
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/Eq(kVaultKeys)));
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());
}

// The server may clean up some stale keys eventually, client should clean them
// up as well to ensure that the state doesn't diverge. In particular, this may
// cause problems with registering authentication factors, since the server will
// reject request with stale keys.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldCleanUpOldKeysWhenDownloadingNew) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  StoreKeysAndMimicRecoveryFactorRegistration(
      {kInitialVaultKey}, kInitialLastKeyVersion, kAccountInfo);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  const std::vector<uint8_t> kNewVaultKey = {2, 3, 5};

  // Note that |fetch_keys_callback| should not receive kInitialVaultKey.
  EXPECT_CALL(fetch_keys_callback, Run(ElementsAre(kNewVaultKey)));

  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectAttemptRecoveryAndRunCallback(
          LocalRecoveryFactor::RecoveryStatus::kSuccess, {kNewVaultKey},
          kInitialLastKeyVersion + 1);
}

// Regression test for crbug.com/1500258: second FetchKeys() is triggered, while
// first is still ongoing (e.g. keys are being downloaded).
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysAndCompleteConcurrentFetches) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  StoreKeysAndMimicRecoveryFactorRegistration(
      {kInitialVaultKey}, kInitialLastKeyVersion, kAccountInfo);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback1;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback1.Get());

  // Mimic second FetchKeys(), note that keys are not downloaded yet and first
  // fetch is not completed.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback2;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback2.Get());

  // Both fetches should be completed once keys are downloaded.
  std::vector<uint8_t> kNewVaultKey = {2, 3, 5};
  EXPECT_CALL(fetch_keys_callback1,
              Run(ElementsAre(kInitialVaultKey, kNewVaultKey)));
  EXPECT_CALL(fetch_keys_callback2,
              Run(ElementsAre(kInitialVaultKey, kNewVaultKey)));

  base::HistogramTester histogram_tester;
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectAttemptRecoveryAndRunCallback(
          LocalRecoveryFactor::RecoveryStatus::kSuccess,
          {kInitialVaultKey, kNewVaultKey}, kInitialLastKeyVersion + 1);

  // Recover keys status should be recorded for every fetch.
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoverKeysOutcome." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultRecoverKeysOutcomeForUMA::kSuccess,
      /*expected_bucket_count=*/2);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysFromFirstRecoveryFactorFirst) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  SetNumLocalRecoveryFactors(2u);
  ResetBackend();

  StoreKeysAndMimicRecoveryFactorRegistration(
      {GetConstantTrustedVaultKey()}, kInitialLastKeyVersion, kAccountInfo);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Fetch should be completed once keys are downloaded.
  const std::vector<uint8_t> kNewVaultKey = {2, 3, 5};
  EXPECT_CALL(fetch_keys_callback, Run(ElementsAre(kVaultKey, kNewVaultKey)));

  std::vector<FakeLocalRecoveryFactor*> recovery_factors =
      GetOrCreateRecoveryFactors(kAccountInfo);
  ASSERT_THAT(recovery_factors, SizeIs(2));
  // First recovery factor should have been called to attempt recovery.
  recovery_factors[0]->ExpectAttemptRecoveryAndRunCallback(
      LocalRecoveryFactor::RecoveryStatus::kSuccess, {kVaultKey, kNewVaultKey},
      kInitialLastKeyVersion + 1);
  // Second recovery factor should not have been called.
  EXPECT_FALSE(recovery_factors[1]->AttemptRecoveryWasCalled());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDownloadKeysFromSecondRecoveryFactorIfFirstFails) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  SetNumLocalRecoveryFactors(2u);
  ResetBackend();

  StoreKeysAndMimicRecoveryFactorRegistration(
      {GetConstantTrustedVaultKey()}, kInitialLastKeyVersion, kAccountInfo);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Fetch should be completed once keys are downloaded.
  const std::vector<uint8_t> kNewVaultKey = {2, 3, 5};
  EXPECT_CALL(fetch_keys_callback, Run(ElementsAre(kVaultKey, kNewVaultKey)));

  std::vector<FakeLocalRecoveryFactor*> recovery_factors =
      GetOrCreateRecoveryFactors(kAccountInfo);
  ASSERT_THAT(recovery_factors, SizeIs(2));
  // First recovery factor should have been called to attempt recovery. Let it
  // fail.
  recovery_factors[0]->ExpectAttemptRecoveryAndRunCallback(
      LocalRecoveryFactor::RecoveryStatus::kFailure, /*new_vault_keys=*/{},
      /*last_vault_key_version=*/0);
  // Second recovery factor should have been called.
  recovery_factors[1]->ExpectAttemptRecoveryAndRunCallback(
      LocalRecoveryFactor::RecoveryStatus::kSuccess, {kVaultKey, kNewVaultKey},
      kInitialLastKeyVersion + 1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldFailWithLastRecoveryFactorStatus) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kInitialLastKeyVersion = 1;

  SetNumLocalRecoveryFactors(2u);
  ResetBackend();

  StoreKeysAndMimicRecoveryFactorRegistration(
      {GetConstantTrustedVaultKey()}, kInitialLastKeyVersion, kAccountInfo);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(kAccountInfo));
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Fetch should be completed once last key downloading failed.
  EXPECT_CALL(fetch_keys_callback, Run(IsEmpty()));

  base::HistogramTester histogram_tester;
  std::vector<FakeLocalRecoveryFactor*> recovery_factors =
      GetOrCreateRecoveryFactors(kAccountInfo);
  ASSERT_THAT(recovery_factors, SizeIs(2));
  // First recovery factor should have been called to attempt recovery. Let it
  // fail.
  recovery_factors[0]->ExpectAttemptRecoveryAndRunCallback(
      LocalRecoveryFactor::RecoveryStatus::kFailure, /*new_vault_keys=*/{},
      /*last_vault_key_version=*/0);
  // Second recovery factor should have been called. Let it also fail.
  recovery_factors[1]->ExpectAttemptRecoveryAndRunCallback(
      LocalRecoveryFactor::RecoveryStatus::kFailure, /*new_vault_keys=*/{},
      /*last_vault_key_version=*/0);
  // Status of the last recovery factor should be recorded.
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoverKeysOutcome." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultRecoverKeysOutcomeForUMA::kFailure,
      /*expected_bucket_count=*/1);
}

// Tests silent recovery factor registration (when no vault keys available yet).
// After successful registration, the client should be able to download keys.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldSilentlyRegisterRecoveryFactorsAndDownloadNewKeys) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kServerConstantKeyVersion = 100;

  // Setting the primary account will trigger recovery factor registration.
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  // Pretend that the registration completed successfully.
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectMaybeRegisterAndRunCallback(
          TrustedVaultRegistrationStatus::kSuccess, kServerConstantKeyVersion,
          /*had_local_keys=*/false);

  // Now the fake recovery factor should be registered.
  EXPECT_TRUE(GetOrCreateRecoveryFactor(kAccountInfo)->IsRegistered());

  // FetchKeys() should trigger keys downloading. Note: unlike tests with
  // following regular key rotation, in this case MarkLocalKeysAsStale() isn't
  // called intentionally.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(kAccountInfo, fetch_keys_callback.Get());

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {{1, 2, 3}};
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/kNewVaultKeys));
  GetOrCreateRecoveryFactor(kAccountInfo)
      ->ExpectAttemptRecoveryAndRunCallback(
          LocalRecoveryFactor::RecoveryStatus::kSuccess, kNewVaultKeys,
          kServerConstantKeyVersion + 1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldRecordMetricsIfAlreadyRegistered) {
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  StoreKeysAndMimicRecoveryFactorRegistration({kVaultKey}, kLastKeyVersion,
                                              kAccountInfo);

  // Mimic restart to be able to test histogram recording.
  ResetBackend();

  base::HistogramTester histogram_tester;
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);
  EXPECT_TRUE(
      GetOrCreateRecoveryFactor(kAccountInfo)->MaybeRegisterWasCalled());
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistrationState." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kAlreadyRegisteredV1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryFactorRegistered." +
          GetRecoveryFactorTypeForUMA(GetOrCreateRecoveryFactor(kAccountInfo)) +
          "." + security_domain_name_for_uma(),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldAddTrustedRecoveryMethod) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2}, {1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  StoreKeysAndMimicRecoveryFactorRegistration(kVaultKeys, kLastKeyVersion,
                                              kAccountInfo);
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(kAccountInfo),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::MockCallback<base::OnceClosure> completion_callback;
  backend()->AddTrustedRecoveryMethod(kAccountInfo.gaia, kPublicKey,
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
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  ASSERT_THAT(SecureBoxPublicKey::CreateByImport(kInvalidPublicKey), IsNull());

  StoreKeysAndMimicRecoveryFactorRegistration(kVaultKeys, kLastKeyVersion,
                                              kAccountInfo);
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);

  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(completion_callback, Run());
  backend()->AddTrustedRecoveryMethod(kAccountInfo.gaia, kInvalidPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferTrustedRecoveryMethodUntilPrimaryAccount) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  backend()->StoreKeys(kAccountInfo.gaia, kVaultKeys, kLastKeyVersion);
  ASSERT_FALSE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // No request should be issued while there is no primary account.
  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->AddTrustedRecoveryMethod(kAccountInfo.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());
  EXPECT_TRUE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // Upon setting a primary account, RegisterAuthenticationFactor() should be
  // invoked.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(kAccountInfo),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        // Note: TrustedVaultConnection::Request doesn't support
        // cancellation, so these tests don't cover the contract that
        // caller should store Request object until it's completed or need
        // to be cancelled.
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  SetPrimaryAccountWithUnknownAuthError(kAccountInfo);

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
  const CoreAccountInfo kAccountInfo = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  // Mimic fake recovery factor previously registered with some keys.
  StoreKeysAndMimicRecoveryFactorRegistration(kVaultKeys, kLastKeyVersion,
                                              kAccountInfo);

  // Mimic entering a persistent auth error.
  backend()->SetPrimaryAccount(
      kAccountInfo, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kPersistentAuthError);

  // No request should be issued while there is a persistent auth error.
  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->AddTrustedRecoveryMethod(kAccountInfo.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());

  EXPECT_TRUE(backend()->HasPendingTrustedRecoveryMethodForTesting());

  // Upon resolving the auth error, the request should be issued.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(kAccountInfo),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKeys}, kLastKeyVersion)),
          PublicKeyWhenExportedEq(kPublicKey),
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              UnspecifiedAuthenticationFactorType(kMethodTypeHint))),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
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
      kAccountInfo, StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                        kNoPersistentAuthErrors);

  // The operation should be in flight.
  EXPECT_FALSE(backend()->HasPendingTrustedRecoveryMethodForTesting());
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);
}

}  // namespace

}  // namespace trusted_vault
