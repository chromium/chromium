// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
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

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock();
  ~PersonalDataLoadedObserverMock() override;

  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));
  MOCK_METHOD(void, OnPersonalDataFinishedProfileTasks, (), (override));
};

class PersonalDataManagerTestBase {
 protected:
  static std::vector<base::test::FeatureRef> GetDefaultEnabledFeatures();

  explicit PersonalDataManagerTestBase(
      const std::vector<base::test::FeatureRef>& additional_enabled_features =
          {});

  ~PersonalDataManagerTestBase();

  void SetUpTest();
  void TearDownTest();

  void ResetPersonalDataManager(bool is_incognito,
                                bool use_sync_transport_mode,
                                PersonalDataManager* personal_data);

  [[nodiscard]] bool TurnOnSyncFeature(PersonalDataManager* personal_data);

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid,
                                           PersonalDataManager* personal_data);

  void SetServerCards(std::vector<CreditCard> server_cards);

  // Verify that the web database has been updated and the notification sent.
  void WaitOnceForOnPersonalDataChanged();

  // Verifies that the web database has been updated and the notification sent.
  void WaitForOnPersonalDataChanged();

  // Verifies that the web database has been updated and the notification sent.
  void WaitForOnPersonalDataChangedRepeatedly();

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  base::test::ScopedFeatureList scoped_features_;
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
  PersonalDataLoadedObserverMock personal_data_observer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_BASE_H_
