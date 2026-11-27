// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"

namespace autofill {

class TestAutofillDriver;

class TestAutofillDriverFactory : public AutofillDriverFactory {
 public:
  using LifecycleState = AutofillDriverFactory::LifecycleState;

  // Suppresses frame-structure checks while alive. Once all live
  // `ScopedValidationSuppressor`s for a factory have been destroyed, the frame
  // structure is re-validated automatically.
  //
  // Use this when incrementally mutating the frame structure through a
  // transiently invalid state (e.g., prerender activation or BFCache restore,
  // where the incoming primary main frame becomes active before the outgoing
  // one is deactivated).
  class [[nodiscard]] ScopedValidationSuppressor {
   public:
    explicit ScopedValidationSuppressor(TestAutofillDriverFactory& factory);
    ScopedValidationSuppressor(const ScopedValidationSuppressor&) = delete;
    ScopedValidationSuppressor& operator=(const ScopedValidationSuppressor&) =
        delete;
    ScopedValidationSuppressor(ScopedValidationSuppressor&&) = delete;
    ScopedValidationSuppressor& operator=(ScopedValidationSuppressor&&) =
        delete;
    ~ScopedValidationSuppressor();

   private:
    base::WeakPtr<TestAutofillDriverFactory> factory_;
  };

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

  // Controls whether `CheckValidFrameTree()` requires an active primary main
  // frame. Defaults to true so tests do not accidentally run without any
  // active frame.
  //
  // Validates the frame tree if the factory owns any drivers. That is, turning
  // the requirement back on CHECK-fails if there is no active primary main
  // frame at that point.
  void RequirePrimaryMainFrame(bool require_primary_main_frame);

  // Validates the frame tree formed by the currently owned drivers.
  //
  // These checks can be suppressed with a ScopedValidationSuppressor (e.g.,
  // during the test setup phase).
  //
  // Tests that intentionally have no active, non-embedded AutofillDrivers can
  // disable that particular check with `RequirePrimaryMainFrame(false)`.
  void CheckValidFrameTree() const;

  // Indicates whether this factory owns `driver`.
  bool Owns(const TestAutofillDriver& driver) const;

 private:
  bool is_validation_suppressed() const {
    return num_validation_suppressors_ > 0;
  }

  std::vector<std::unique_ptr<TestAutofillDriver>> drivers_;
  bool require_primary_main_frame_ = true;
  size_t num_validation_suppressors_ = 0;
  base::WeakPtrFactory<TestAutofillDriverFactory> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
