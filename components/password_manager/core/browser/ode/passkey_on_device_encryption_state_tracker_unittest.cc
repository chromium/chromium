// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/passkey_on_device_encryption_state_tracker.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

class TestPasskeyOnDeviceEncryptionStateTracker
    : public PasskeyOnDeviceEncryptionStateTracker {
 public:
  TestPasskeyOnDeviceEncryptionStateTracker(
      syncer::SyncService* sync_service,
      webauthn::PasskeyModel* passkey_model)
      : PasskeyOnDeviceEncryptionStateTracker(sync_service, passkey_model) {
    ComputeState();
  }

  void SetPlatformState(OnDeviceEncryptionState state) {
    platform_state_ = state;
    ComputeState();
  }

 protected:
  OnDeviceEncryptionState GetPlatformState() const override {
    return platform_state_;
  }

 private:
  OnDeviceEncryptionState platform_state_ =
      OnDeviceEncryptionState::kDeviceReady;
};

enum class AccountState {
  kNotSignedIn,
  kSignInPending,
  kSignedIn,
};

std::string_view AccountStateToString(AccountState state) {
  switch (state) {
    case AccountState::kNotSignedIn:
      return "ProfileNotSignedIn";
    case AccountState::kSignInPending:
      return "ProfileSignInPending";
    case AccountState::kSignedIn:
      return "ProfileSignedIn";
  }
}

// Used in parameterized tests which verify that the correct state is being
// derived for different combinations of the parameters.
struct StateComputationTestCase {
  AccountState account_state;
  bool is_sync_engine_initialized;
  bool is_webauthn_credential_sync_enabled;
  bool is_passkey_model_ready;
  bool is_passkey_model_empty;
  OnDeviceEncryptionState platform_state;
  OnDeviceEncryptionState expected_state;
};

class PasskeyOnDeviceEncryptionStateTrackerStateTest
    : public testing::TestWithParam<
          std::tuple<AccountState /*account_state*/,
                     bool /*is_sync_engine_initialized*/,
                     bool /*is_webauthn_credential_sync_enabled*/,
                     bool /*is_passkey_model_ready*/,
                     bool /*is_passkey_model_empty*/,
                     OnDeviceEncryptionState /*platform_state*/,
                     OnDeviceEncryptionState /*expected_state*/>> {
 public:
  StateComputationTestCase GetTestCase() const {
    return StateComputationTestCase{
        .account_state = std::get<0>(GetParam()),
        .is_sync_engine_initialized = std::get<1>(GetParam()),
        .is_webauthn_credential_sync_enabled = std::get<2>(GetParam()),
        .is_passkey_model_ready = std::get<3>(GetParam()),
        .is_passkey_model_empty = std::get<4>(GetParam()),
        .platform_state = std::get<5>(GetParam()),
        .expected_state = std::get<6>(GetParam()),
    };
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

std::string StateToString(OnDeviceEncryptionState state) {
  switch (state) {
    case OnDeviceEncryptionState::kDeviceReady:
      return "DeviceReady";
    case OnDeviceEncryptionState::kDeviceNotReady:
      return "DeviceNotReady";
    case OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable:
      return "NotAvailable";
    case OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled:
      return "SyncDisabled";
    case OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled:
      return "NotEnabled";
    case OnDeviceEncryptionState::kProfileNotSignedIn:
      return "ProfileNotSignedIn";
    case OnDeviceEncryptionState::kProfileSignInPending:
      return "ProfileSignInPending";
  }
}

// Used for creating human-readable names for parameterized test cases.
std::string ParamInfoToString(
    const testing::TestParamInfo<
        PasskeyOnDeviceEncryptionStateTrackerStateTest::ParamType>& info) {
  return base::StrCat({
      AccountStateToString(std::get<0>(info.param)),
      "_",
      std::get<1>(info.param) ? "SyncEngineInitialized_"
                              : "SyncEngineNotInitialized_",
      std::get<2>(info.param) ? "WebauthnSyncEnabled_"
                              : "WebauthnSyncDisabled_",
      std::get<3>(info.param) ? "PasskeyModelReady_" : "PasskeyModelNotReady_",
      std::get<4>(info.param) ? "PasskeysEmpty_" : "PasskeysExist_",
      "PlatformState_",
      StateToString(std::get<5>(info.param)),
  });
}

TEST_P(PasskeyOnDeviceEncryptionStateTrackerStateTest, ComputesCorrectState) {
  const StateComputationTestCase test_case = GetTestCase();

  syncer::TestSyncService sync_service;
  switch (test_case.account_state) {
    case AccountState::kNotSignedIn:
      sync_service.SetSignedOut();
      break;
    case AccountState::kSignInPending:
      // Setting a persistent auth error puts sync into the `PAUSED` state.
      sync_service.SetPersistentAuthError();
      break;
    case AccountState::kSignedIn:
      break;
  }
  // For `kSignInPending`, sync must stay in `PAUSED` state, so do not override
  // it with `INITIALIZING`.
  if (!test_case.is_sync_engine_initialized &&
      test_case.account_state != AccountState::kSignInPending) {
    sync_service.SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
  }
  if (test_case.is_webauthn_credential_sync_enabled) {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        // `kPasswords` indicates that syncing of passkeys and passwords is
        // active.
        /*types=*/{syncer::UserSelectableType::kPasswords});
  } else {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{});
  }

  webauthn::TestPasskeyModel passkey_model;
  passkey_model.SetReady(test_case.is_passkey_model_ready);
  if (!test_case.is_passkey_model_empty) {
    passkey_model.CreatePasskey(
        "example.com",
        webauthn::PasskeyModel::UserEntity({1, 2, 3}, "user", "User"),
        /*trusted_vault_key=*/{}, /*trusted_vault_key_version=*/0,
        /*public_key_spki_der_out=*/nullptr);
  }

  TestPasskeyOnDeviceEncryptionStateTracker tracker(&sync_service,
                                                    &passkey_model);
  tracker.SetPlatformState(test_case.platform_state);
  EXPECT_EQ(tracker.GetEncryptionState(), test_case.expected_state);
}

// When the profile is not signed in, the on-device encryption state is
// "profile not signed in".
INSTANTIATE_TEST_SUITE_P(
    ProfileNotSignedIn,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kNotSignedIn),
        /*is_sync_engine_initialized=*/Values(false),
        /*is_webauthn_credential_sync_enabled=*/Bool(),
        /*is_passkey_model_ready=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kProfileNotSignedIn)),
    &ParamInfoToString);

// When the profile is in sign-in pending state (sync is paused), the on-device
// encryption state is "sign-in pending".
INSTANTIATE_TEST_SUITE_P(
    ProfileSignInPending,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignInPending),
        /*is_sync_engine_initialized=*/Values(false),
        /*is_webauthn_credential_sync_enabled=*/Bool(),
        /*is_passkey_model_ready=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kProfileSignInPending)),
    &ParamInfoToString);

// When sync engine is not initialized the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    SyncEngineNotInitialized,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(false),
        /*is_webauthn_credential_sync_enabled=*/Bool(),
        /*is_passkey_model_ready=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// Testing the cases when password and passkey sync is disabled.
INSTANTIATE_TEST_SUITE_P(
    WebauthnCredentialSyncNotEnabled,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(false),
        /*is_passkey_model_ready=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled)),
    &ParamInfoToString);

// When passkey model is not ready the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    PasskeyModelNotReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(false),
        /*is_passkey_model_empty=*/Bool(),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// When passkey model is empty (0 passkeys) we assume that the on-device
// encryption is not enabled.
INSTANTIATE_TEST_SUITE_P(
    PasskeyModelEmpty,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_passkey_model_empty=*/Values(true),
        /*platform_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady,
               OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled)),
    &ParamInfoToString);

// When passkeys exist, tracker returns the platform state.
INSTANTIATE_TEST_SUITE_P(
    DeviceReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_passkey_model_empty=*/Values(false),
        /*platform_state=*/Values(OnDeviceEncryptionState::kDeviceReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady)),
    &ParamInfoToString);

INSTANTIATE_TEST_SUITE_P(
    DeviceNotReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_passkey_model_empty=*/Values(false),
        /*platform_state=*/Values(OnDeviceEncryptionState::kDeviceNotReady),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceNotReady)),
    &ParamInfoToString);

}  // namespace

}  // namespace password_manager
