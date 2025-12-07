// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_driver_factory.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"

namespace autofill {

namespace {

template <typename T>
  requires(std::derived_from<TestAutofillDriver, T>)
T* ToPointer(const std::unique_ptr<TestAutofillDriver>& driver) {
  return driver.get();
}

}  // namespace

TestAutofillDriverFactory::TestAutofillDriverFactory() = default;

TestAutofillDriverFactory::~TestAutofillDriverFactory() {
  for (auto& observer : AutofillDriverFactory::observers()) {
    observer.OnAutofillDriverFactoryDestroyed(*this);
  }
}

bool TestAutofillDriverFactory::IsOwned(TestAutofillDriver& driver) const {
  return base::Contains(drivers_, &driver, &ToPointer<TestAutofillDriver>);
}

TestAutofillDriver& TestAutofillDriverFactory::TakeOwnership(
    std::unique_ptr<TestAutofillDriver> driver) {
  TestAutofillDriver& raw_driver = *driver;
  drivers_.push_back(std::move(driver));
  for (auto& observer : observers()) {
    observer.OnAutofillDriverCreated(*this, raw_driver);
  }
  return raw_driver;
}

void TestAutofillDriverFactory::Activate(TestAutofillDriver& driver) {
  CHECK(IsOwned(driver));
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kActive);
}

void TestAutofillDriverFactory::Deactivate(TestAutofillDriver& driver) {
  CHECK(IsOwned(driver));
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kInactive);
}

void TestAutofillDriverFactory::Reset(TestAutofillDriver& driver) {
  CHECK(IsOwned(driver));
  LifecycleState original_state = driver.GetLifecycleState();
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kPendingReset);
  SetLifecycleStateAndNotifyObservers(driver, original_state);
}

void TestAutofillDriverFactory::Delete(TestAutofillDriver& driver) {
  CHECK(IsOwned(driver));
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kPendingDeletion);
  auto it =
      std::ranges::find(drivers_, &driver, &ToPointer<TestAutofillDriver>);
  drivers_.erase(it);
}

void TestAutofillDriverFactory::DeleteAll() {
  for (const std::unique_ptr<TestAutofillDriver>& driver : drivers_) {
    SetLifecycleStateAndNotifyObservers(*driver,
                                        LifecycleState::kPendingDeletion);
  }
  drivers_.clear();
}

std::vector<AutofillDriver*> TestAutofillDriverFactory::GetExistingDrivers() {
  return base::ToVector(drivers_, &ToPointer<AutofillDriver>);
}

TestAutofillDriver* TestAutofillDriverFactory::driver(size_t index) {
  return index < drivers_.size() ? drivers_[index].get() : nullptr;
}

}  // namespace autofill
