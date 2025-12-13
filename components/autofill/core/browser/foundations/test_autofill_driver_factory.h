// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_

#include <memory>
#include <vector>

#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"

namespace autofill {

class TestAutofillDriver;

class TestAutofillDriverFactory : public AutofillDriverFactory {
 public:
  using LifecycleState = AutofillDriverFactory::LifecycleState;

  TestAutofillDriverFactory();
  ~TestAutofillDriverFactory() override;

  // Adopts `driver` into the factory.
  // The factory doesn't provide a way of instantiating drivers because test
  // fixtures may use derived classes (e.g., MockAutofillDriver).
  TestAutofillDriver& TakeOwnership(std::unique_ptr<TestAutofillDriver> driver);

  // See AutofillDriver::LifecycleState for documentation.
  void Activate(TestAutofillDriver& driver);
  void Deactivate(TestAutofillDriver& driver);
  void Reset(TestAutofillDriver& driver);
  void Delete(TestAutofillDriver& driver);

  void DeleteAll();

  size_t num_drivers() const { return drivers_.size(); }

  TestAutofillDriver* driver(size_t index = 0);

  std::vector<AutofillDriver*> GetExistingDrivers() override;

 private:
  bool IsOwned(TestAutofillDriver& driver) const;

  std::vector<std::unique_ptr<TestAutofillDriver>> drivers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
