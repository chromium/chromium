// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class PersonalDataManager;

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock();
  ~PersonalDataLoadedObserverMock() override;

  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));
};

// Helper class to wait for an `OnPersonalDataChanged()` call from the `pdm`.
// This is necessary, since the PDM operates asynchronously on the WebDatabase.
// Example usage:
//   PersonalDataManagerWaiter waiter(pdm);
//   pdm.AddProfile(AutofillProfile());
//   std::move(waiter).Wait();
//
// Initializing the waiter after the operation (`AddProfile()`, in this case) is
// not recommended, because the notifications might fire before the expectations
// are set.
class PersonalDataChangedWaiter {
 public:
  explicit PersonalDataChangedWaiter(PersonalDataManager& pdm);
  ~PersonalDataChangedWaiter();

  // Waits for `OnPersonalDataChanged()` to trigger. As a safety mechanism, this
  // can only be called once per `PersonalDataChangedWaiter` instance. This
  // is because gMock doesn't support setting expectations after a function
  // (here the mock_observer_'s `OnPersonalDataChanged()`) was called.
  void Wait() &&;

 private:
  testing::NiceMock<PersonalDataLoadedObserverMock> mock_observer_;
  base::RunLoop run_loop_;
  base::ScopedObservation<PersonalDataManager, PersonalDataLoadedObserverMock>
      scoped_observation_{&mock_observer_};
};

// Operations on the PDM like "adding a profile" asynchronously update the
// database. In such cases, it generally suffices to wait for the operation to
// complete using `PersonalDataChangedWaiter` above.
// But in cases where it is unclear if a profile was added, for example during
// a form submission in browser tests, this doesn't work; if no profile was
// added, the `PersonalDataChangedWaiter` would wait forever.
// In this case, `WaitForPendingDBTasks()` can help: It queues a task to the
// WebDataService's SequencedTaskRunner and returns once it has executed,
// implying that all the prior tasks have finished. In the case of form
// submission, if no profile was added, it will thus return immediately.
// If possible, prefer `PersonalDataChangedWaiter`, since it is more
// explicit which event is waited for.
void WaitForPendingDBTasks(AutofillWebDataService& webdata_service);

// Signs in through the `identity_test_env` and makes the primary account
// info available to the `sync_service`. Depending on
// `use_sync_transport_mode`, sync-the-feature is either activated or not.
void MakePrimaryAccountAvailable(
    bool use_sync_transport_mode,
    signin::IdentityTestEnvironment& identity_test_env,
    syncer::TestSyncService& sync_service);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_UTILS_H_
