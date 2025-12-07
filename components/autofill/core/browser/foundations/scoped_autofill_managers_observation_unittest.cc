// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::Ref;

class ScopedAutofillManagersObservationTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  using enum AutofillManager::LifecycleState;

  ScopedAutofillManagersObservationTest() { InitAutofillClient(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ScopedAutofillManagersObservationTest, SingleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client());
  CreateAutofillDriver();

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(autofill_manager()),
                                                      kInactive, kActive));
  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(autofill_manager())));
  ActivateAutofillDriver(autofill_driver());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitializationFailsWithStrictPolicy) {
  CreateAutofillDriver();
  MockAutofillManagerObserver observer;

  ScopedAutofillManagersObservation observation(&observer);
  EXPECT_CHECK_DEATH(observation.Observe(
      &autofill_client(),
      ScopedAutofillManagersObservation::InitializationPolicy::
          kExpectNoPreexistingManagers));
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitialization) {
  CreateAutofillDriver();
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client(),
                      ScopedAutofillManagersObservation::InitializationPolicy::
                          kObservePreexistingManagers);

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(autofill_manager())));
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, NoObservationsAfterReset) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client());
  CreateAutofillDriver();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(autofill_manager())));
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);

  observation.Reset();
  EXPECT_CALL(observer, OnBeforeLanguageDetermined).Times(0);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, MultipleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client());
  CreateAutofillDriver();
  CreateAutofillDriver();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(autofill_manager(0))));
  autofill_manager(0).NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(autofill_manager(1))));
  autofill_manager(1).NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       StateChangedToPendingResetNotifiesObserver) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client());
  CreateAutofillDriver();

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(autofill_manager()),
                                                      kInactive, kActive));
  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(autofill_manager()),
                                                      kActive, kPendingReset));
  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(autofill_manager()),
                                                      kPendingReset, kActive));
  ActivateAutofillDriver(autofill_driver());
  ResetAutofillDriver(autofill_driver());
}

TEST_F(ScopedAutofillManagersObservationTest,
       StateChangedToPendingDeletionNotifiesObserver) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&autofill_client());
  CreateAutofillDriver();

  EXPECT_CALL(observer,
              OnAutofillManagerStateChanged(Ref(autofill_manager()), kInactive,
                                            kPendingDeletion));
  DeleteAutofillDriver(autofill_driver());
}

}  // namespace autofill
