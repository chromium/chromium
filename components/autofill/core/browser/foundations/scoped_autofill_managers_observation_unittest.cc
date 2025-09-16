// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

#include "base/containers/to_vector.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Ref;

class ScopedAutofillManagersObservationTest : public testing::Test {
 protected:
  using Observer = AutofillManager::Observer;
  using enum AutofillManager::LifecycleState;

  TestAutofillClient& client() { return client_; }

  TestAutofillDriverFactory& factory() {
    return client().GetAutofillDriverFactory();
  }

  TestAutofillDriver& driver(size_t index = 0) {
    return *factory().driver(index);
  }

  AutofillManager& manager(size_t index = 0) {
    return driver(index).GetAutofillManager();
  }

  void CreateDriver() {
    auto driver = std::make_unique<TestAutofillDriver>(&client());
    driver->set_autofill_manager(
        std::make_unique<TestBrowserAutofillManager>(driver.get()));
    factory().TakeOwnership(std::move(driver));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillClient client_;
};

TEST_F(ScopedAutofillManagersObservationTest, SingleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client());
  CreateDriver();

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(manager()), kInactive,
                                                      kActive));
  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(manager())));
  factory().Activate(driver());
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitializationFailsWithStrictPolicy) {
  CreateDriver();
  MockAutofillManagerObserver observer;

  ScopedAutofillManagersObservation observation(&observer);
  EXPECT_CHECK_DEATH(observation.Observe(
      &client(), ScopedAutofillManagersObservation::InitializationPolicy::
                     kExpectNoPreexistingManagers));
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitialization) {
  CreateDriver();
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client(),
                      ScopedAutofillManagersObservation::InitializationPolicy::
                          kObservePreexistingManagers);

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(manager())));
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, NoObservationsAfterReset) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client());
  CreateDriver();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(manager())));
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);

  observation.Reset();
  EXPECT_CALL(observer, OnBeforeLanguageDetermined).Times(0);
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, MultipleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client());
  CreateDriver();
  CreateDriver();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(manager(0))));
  manager(0).NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(manager(1))));
  manager(1).NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       StateChangedToPendingResetNotifiesObserver) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client());
  CreateDriver();

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(manager()), kInactive,
                                                      kActive));
  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(manager()), kActive,
                                                      kPendingReset));
  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(manager()),
                                                      kPendingReset, kActive));
  factory().Activate(driver());
  factory().Reset(driver());
}

TEST_F(ScopedAutofillManagersObservationTest,
       StateChangedToPendingDeletionNotifiesObserver) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(&client());
  CreateDriver();

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(Ref(manager()), kInactive,
                                                      kPendingDeletion));
  factory().Delete(driver());
}

}  // namespace autofill
