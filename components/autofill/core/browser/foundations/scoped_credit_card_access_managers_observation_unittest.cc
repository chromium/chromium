// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/scoped_credit_card_access_managers_observation.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/mock_credit_card_access_manager_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::Ref;

class ScopedCreditCardAccessManagersObservationTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  ScopedCreditCardAccessManagersObservationTest() { InitAutofillClient(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ScopedCreditCardAccessManagersObservationTest, IsObserving) {
  MockCreditCardAccessManagerObserver observer;
  ScopedCreditCardAccessManagersObservation observation(&observer);

  EXPECT_FALSE(observation.IsObserving());
  observation.Observe(&autofill_client());
  EXPECT_TRUE(observation.IsObserving());
  observation.Reset();
  EXPECT_FALSE(observation.IsObserving());
}

TEST_F(ScopedCreditCardAccessManagersObservationTest, SingleFrameObservation) {
  MockCreditCardAccessManagerObserver observer;
  ScopedCreditCardAccessManagersObservation observation(&observer);

  observation.Observe(&autofill_client());
  CreateAutofillDriver();
  ActivateAutofillDriver(autofill_driver());

  EXPECT_CALL(observer, OnCreditCardAccessManagerDestroyed(Ref(
                            *autofill_manager().GetCreditCardAccessManager())));
  DeleteAutofillDriver(autofill_driver());
}

TEST_F(ScopedCreditCardAccessManagersObservationTest,
       SingleFrameObservationDelayedInitialization) {
  MockCreditCardAccessManagerObserver observer;
  ScopedCreditCardAccessManagersObservation observation(&observer);

  CreateAutofillDriver();
  ActivateAutofillDriver(autofill_driver());
  observation.Observe(&autofill_client());

  EXPECT_CALL(observer, OnCreditCardAccessManagerDestroyed(Ref(
                            *autofill_manager().GetCreditCardAccessManager())));
  DeleteAutofillDriver(autofill_driver());
}

TEST_F(ScopedCreditCardAccessManagersObservationTest, Reset) {
  MockCreditCardAccessManagerObserver observer;
  ScopedCreditCardAccessManagersObservation observation(&observer);

  observation.Observe(&autofill_client());
  CreateAutofillDriver();
  ActivateAutofillDriver(autofill_driver());

  EXPECT_CALL(observer, OnCreditCardAccessManagerDestroyed).Times(0);
  observation.Reset();
  DeleteAutofillDriver(autofill_driver());
}

TEST_F(ScopedCreditCardAccessManagersObservationTest,
       MultipleFrameObservation) {
  MockCreditCardAccessManagerObserver observer;
  ScopedCreditCardAccessManagersObservation observation(&observer);

  observation.Observe(&autofill_client());
  CreateAutofillDriver();
  CreateAutofillDriver();
  ActivateAutofillDriver(autofill_driver(0));
  ActivateAutofillDriver(autofill_driver(1));

  EXPECT_CALL(observer,
              OnCreditCardAccessManagerDestroyed(
                  Ref(*autofill_manager(0).GetCreditCardAccessManager())));
  EXPECT_CALL(observer,
              OnCreditCardAccessManagerDestroyed(
                  Ref(*autofill_manager(1).GetCreditCardAccessManager())));

  DeleteAutofillDriver(autofill_driver(0));
  DeleteAutofillDriver(autofill_driver(0));
}

}  // namespace autofill
