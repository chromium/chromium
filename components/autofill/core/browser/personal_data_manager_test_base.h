// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class PersonalDataManager;

class PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerTestBase();

  ~PersonalDataManagerTestBase();

  void SetUpTest();
  void TearDownTest();

  void ResetPersonalDataManager(bool use_sync_transport_mode,
                                PersonalDataManager* personal_data);

  [[nodiscard]] bool TurnOnSyncFeature(PersonalDataManager* personal_data);

  void SetServerCards(std::vector<CreditCard> server_cards);

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<AutofillWebDataService> profile_database_service_;
  scoped_refptr<AutofillWebDataService> account_database_service_;
  scoped_refptr<WebDatabaseService> profile_web_database_;
  scoped_refptr<WebDatabaseService> account_web_database_;
  raw_ptr<AutofillTable> profile_autofill_table_;  // weak ref
  raw_ptr<AutofillTable> account_autofill_table_;  // weak ref
  std::unique_ptr<StrikeDatabaseBase> strike_database_;
  testing::NiceMock<PersonalDataLoadedObserverMock> personal_data_observer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
