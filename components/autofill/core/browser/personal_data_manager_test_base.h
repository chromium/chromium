// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"

namespace autofill {

class PersonalDataManager;

class PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerTestBase();

  ~PersonalDataManagerTestBase();

  void SetUpTest();
  void TearDownTest();

  // Signs in through the `identity_test_env_` and makes the primary account
  // info available to the `sync_service_`. Depending on
  // `use_sync_transport_mode`, sync-the-feature is either activated or not.
  void MakePrimaryAccountAvailable(bool use_sync_transport_mode);

  // Calls `MakePrimaryAccountAvailable()`, initializes a PersonalDataManager
  // and waits for the `Refresh()` to complete.
  std::unique_ptr<PersonalDataManager> InitPersonalDataManager(
      bool use_sync_transport_mode);

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<AutofillWebDataService> profile_database_service_;
  scoped_refptr<AutofillWebDataService> account_database_service_;
  scoped_refptr<WebDatabaseService> profile_web_database_;
  scoped_refptr<WebDatabaseService> account_web_database_;
  raw_ptr<PaymentsAutofillTable> profile_autofill_table_;  // weak ref
  raw_ptr<PaymentsAutofillTable> account_autofill_table_;  // weak ref
  std::unique_ptr<StrikeDatabaseBase> strike_database_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
