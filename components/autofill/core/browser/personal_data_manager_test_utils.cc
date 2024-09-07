// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_test_utils.h"

#include "base/test/gmock_callback_support.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

namespace {

const char kPrimaryAccountEmail[] = "syncuser@example.com";
const char kSyncTransportAccountEmail[] = "transport@example.com";

}  // anonymous namespace

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() = default;
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() = default;

PersonalDataChangedWaiter::PersonalDataChangedWaiter(PersonalDataManager& pdm) {
  scoped_observation_.Observe(&pdm);
  ON_CALL(mock_observer_, OnPersonalDataChanged())
      .WillByDefault(base::test::RunClosure(run_loop_.QuitClosure()));
}

PersonalDataChangedWaiter::~PersonalDataChangedWaiter() = default;

void PersonalDataChangedWaiter::Wait() && {
  run_loop_.Run();
}

void WaitForPendingDBTasks(AutofillWebDataService& webdata_service) {
  base::RunLoop run_loop;
  webdata_service.GetDBTaskRunner()->PostTask(FROM_HERE,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

void MakePrimaryAccountAvailable(
    bool use_sync_transport_mode,
    signin::IdentityTestEnvironment& identity_test_env,
    syncer::TestSyncService& sync_service) {
  std::string email = use_sync_transport_mode ? kSyncTransportAccountEmail
                                              : kPrimaryAccountEmail;
  // Set the account in both IdentityManager and SyncService.
  CoreAccountInfo account_info;
  signin::ConsentLevel consent_level = use_sync_transport_mode
                                           ? signin::ConsentLevel::kSignin
                                           : signin::ConsentLevel::kSync;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  identity_test_env.ClearPrimaryAccount();
  account_info =
      identity_test_env.MakePrimaryAccountAvailable(email, consent_level);
#else
  // In ChromeOS-Ash, clearing/resetting the primary account is not supported.
  // So if an account already exists, reuse it (and make sure it matches).
  if (identity_test_env.identity_manager()->HasPrimaryAccount(consent_level)) {
    account_info = identity_test_env.identity_manager()->GetPrimaryAccountInfo(
        consent_level);
    ASSERT_EQ(account_info.email, email);
  } else {
    account_info =
        identity_test_env.MakePrimaryAccountAvailable(email, consent_level);
  }
#endif
  sync_service.SetSignedIn(use_sync_transport_mode
                               ? signin::ConsentLevel::kSignin
                               : signin::ConsentLevel::kSync,
                           account_info);
}

}  // namespace autofill
