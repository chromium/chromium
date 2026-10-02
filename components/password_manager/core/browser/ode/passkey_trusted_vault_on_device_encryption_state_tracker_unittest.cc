// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/passkey_trusted_vault_on_device_encryption_state_tracker.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ode/mock_on_device_encryption_state_tracker_observer.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::StrictMock;
using ::testing::Values;

std::vector<std::vector<uint8_t>> GetTestKeys() {
  return {{1, 2, 3}};
}

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

enum class SelectedSyncTypes {
  kNone,
  kPasswordsOnly,
  kSyncEverything,
};

std::string_view SelectedSyncTypesToString(SelectedSyncTypes state) {
  switch (state) {
    case SelectedSyncTypes::kNone:
      return "SyncTypesNone";
    case SelectedSyncTypes::kPasswordsOnly:
      return "SyncTypesPasswordsOnly";
    case SelectedSyncTypes::kSyncEverything:
      return "SyncEverything";
  }
}

// Used in parameterized tests which verify that the correct state is being
// derived for different combinations of the parameters.
struct StateComputationTestCase {
  AccountState account_state;
  bool is_sync_engine_initialized;
  SelectedSyncTypes selected_sync_types;
  bool is_key_fetch_completed;
  bool are_keys_available;
  OnDeviceEncryptionState expected_state;
};

class PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest
    : public testing::TestWithParam<
          std::tuple<AccountState /*account_state*/,
                     bool /*is_sync_engine_initialized*/,
                     SelectedSyncTypes /*selected_sync_types*/,
                     bool /*is_key_fetch_completed*/,
                     bool /*are_keys_available*/,
                     OnDeviceEncryptionState /*expected_state*/>> {
 public:
  StateComputationTestCase GetTestCase() const {
    return StateComputationTestCase{
        .account_state = std::get<0>(GetParam()),
        .is_sync_engine_initialized = std::get<1>(GetParam()),
        .selected_sync_types = std::get<2>(GetParam()),
        .is_key_fetch_completed = std::get<3>(GetParam()),
        .are_keys_available = std::get<4>(GetParam()),
        .expected_state = std::get<5>(GetParam()),
    };
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Used for creating human-readable names for parameterized test cases.
std::string ParamInfoToString(
    const testing::TestParamInfo<
        PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest::ParamType>&
        info) {
  return base::StrCat({
      AccountStateToString(std::get<0>(info.param)),
      "_",
      std::get<1>(info.param) ? "SyncEngineInitialized_"
                              : "SyncEngineNotInitialized_",
      SelectedSyncTypesToString(std::get<2>(info.param)),
      "_",
      std::get<3>(info.param) ? "KeyFetchCompleted_" : "KeyFetchInFlight_",
      std::get<4>(info.param) ? "KeysAvailable" : "KeysNotAvailable",
  });
}

TEST_P(PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
       ComputesCorrectState) {
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
  switch (test_case.selected_sync_types) {
    case SelectedSyncTypes::kNone:
      sync_service.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{});
      break;
    case SelectedSyncTypes::kPasswordsOnly:
      sync_service.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          // `kPasswords` indicates that syncing of passkeys and passwords is
          // active.
          /*types=*/{syncer::UserSelectableType::kPasswords});
      break;
    case SelectedSyncTypes::kSyncEverything:
      sync_service.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/true,
          /*types=*/syncer::UserSelectableTypeSet::All());
      break;
  }

  trusted_vault::FakeTrustedVaultClient trusted_vault_client;
  if (test_case.are_keys_available &&
      !sync_service.GetAccountInfo().IsEmpty()) {
    trusted_vault_client.StoreKeys(sync_service.GetAccountInfo().gaia,
                                   GetTestKeys(), /*last_key_version=*/1,
                                   /*trigger=*/std::nullopt);
  }

  PasskeyTrustedVaultOnDeviceEncryptionStateTracker tracker(
      &sync_service, &trusted_vault_client);
  if (test_case.is_key_fetch_completed) {
    trusted_vault_client.CompleteAllPendingRequests();
  }
  EXPECT_EQ(tracker.GetEncryptionState(), test_case.expected_state);
}

// When the profile is not signed in, the on-device encryption state is
// "profile not signed in".
INSTANTIATE_TEST_SUITE_P(
    ProfileNotSignedIn,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kNotSignedIn),
        /*is_sync_engine_initialized=*/Values(false),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kNone,
               SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Bool(),
        /*are_keys_available=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kProfileNotSignedIn)),
    &ParamInfoToString);

// When the profile is in sign-in pending state (sync is paused), the on-device
// encryption state is "sign-in pending".
INSTANTIATE_TEST_SUITE_P(
    ProfileSignInPending,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignInPending),
        /*is_sync_engine_initialized=*/Values(false),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kNone,
               SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Bool(),
        /*are_keys_available=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kProfileSignInPending)),
    &ParamInfoToString);

// When sync engine is not initialized the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    SyncEngineNotInitialized,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(false),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kNone,
               SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Bool(),
        /*are_keys_available=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// Testing the cases when password and passkey sync is disabled.
INSTANTIATE_TEST_SUITE_P(
    WebauthnCredentialSyncNotEnabled,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*selected_sync_types=*/Values(SelectedSyncTypes::kNone),
        /*is_key_fetch_completed=*/Bool(),
        /*are_keys_available=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled)),
    &ParamInfoToString);

// While `TrustedVaultClient::FetchKeys()` is in flight, the on-device
// encryption state is not yet available.
INSTANTIATE_TEST_SUITE_P(
    KeyFetchInFlight,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Values(false),
        /*are_keys_available=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// When key fetch completes with no keys available, the device is not ready.
INSTANTIATE_TEST_SUITE_P(
    DeviceNotReady,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Values(true),
        /*are_keys_available=*/Values(false),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceNotReady)),
    &ParamInfoToString);

// When key fetch completes with keys available, the device is ready.
INSTANTIATE_TEST_SUITE_P(
    DeviceReady,
    PasskeyTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*account_state=*/Values(AccountState::kSignedIn),
        /*is_sync_engine_initialized=*/Values(true),
        /*selected_sync_types=*/
        Values(SelectedSyncTypes::kPasswordsOnly,
               SelectedSyncTypes::kSyncEverything),
        /*is_key_fetch_completed=*/Values(true),
        /*are_keys_available=*/Values(true),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady)),
    &ParamInfoToString);

class PasskeyTrustedVaultOnDeviceEncryptionStateTrackerTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  trusted_vault::FakeTrustedVaultClient trusted_vault_client_;
  StrictMock<MockOnDeviceEncryptionStateTrackerObserver> observer_;
};

TEST_F(PasskeyTrustedVaultOnDeviceEncryptionStateTrackerTest, NullServices) {
  PasskeyTrustedVaultOnDeviceEncryptionStateTracker tracker_null_sync(
      nullptr, &trusted_vault_client_);
  EXPECT_EQ(tracker_null_sync.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  PasskeyTrustedVaultOnDeviceEncryptionStateTracker tracker_null_vault(
      &sync_service_, nullptr);
  EXPECT_EQ(tracker_null_vault.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST_F(PasskeyTrustedVaultOnDeviceEncryptionStateTrackerTest,
       TransitionsFromNotReadyToReadyWhenKeysChange) {
  PasskeyTrustedVaultOnDeviceEncryptionStateTracker tracker(
      &sync_service_, &trusted_vault_client_);
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceNotReady);

  base::ScopedObservation<OnDeviceEncryptionStateTracker,
                          OnDeviceEncryptionStateTracker::Observer>
      observation(&observer_);
  observation.Observe(&tracker);

  // Storing keys triggers OnTrustedVaultKeysChanged(), which resets cached
  // state to kOnDeviceEncryptionStateNotAvailable while re-fetching.
  EXPECT_CALL(
      observer_,
      OnDeviceEncryptionStateChanged(
          OnDeviceEncryptionState::kDeviceNotReady,
          OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable));
  trusted_vault_client_.StoreKeys(sync_service_.GetAccountInfo().gaia,
                                  GetTestKeys(), /*last_key_version=*/1,
                                  /*trigger=*/std::nullopt);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  EXPECT_CALL(observer_,
              OnDeviceEncryptionStateChanged(
                  OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable,
                  OnDeviceEncryptionState::kDeviceReady));
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);
}

TEST_F(PasskeyTrustedVaultOnDeviceEncryptionStateTrackerTest, SyncShutdown) {
  trusted_vault_client_.StoreKeys(sync_service_.GetAccountInfo().gaia,
                                  GetTestKeys(), /*last_key_version=*/1,
                                  /*trigger=*/std::nullopt);
  PasskeyTrustedVaultOnDeviceEncryptionStateTracker tracker(
      &sync_service_, &trusted_vault_client_);
  EXPECT_TRUE(trusted_vault_client_.CompleteAllPendingRequests());
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);

  sync_service_.Shutdown();
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

}  // namespace

}  // namespace password_manager
