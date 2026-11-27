// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"

namespace autofill {

class AutofillManager;
class TestAutofillClient;
class TestAutofillClientBase;
class TestAutofillDriver;
class TestBrowserAutofillManager;

namespace internal {

// Configuration for `TestAutofillDriverFactory::Create()` and
// `CreateAndActivate()`. Defined outside `TestAutofillDriverFactory` so it can
// be used as a default argument on member functions of the class.
struct TestAutofillDriverCreateParams {
  STACK_ALLOCATED();

 public:
  TestAutofillDriver* parent = nullptr;
  bool is_embedded = false;
};

}  // namespace internal

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

  // `client` is the TestAutofillClient that owns this factory.
  TestAutofillDriverFactory(TestAutofillClientBase* client,
                            base::PassKey<TestAutofillClientBase>);
  ~TestAutofillDriverFactory() override;

  using CreateParams = internal::TestAutofillDriverCreateParams;

  // Creates a `DriverT` and a `ManagerT` for it. The factory owns the new
  // driver, which is initially inactive.
  //
  // The constructor of `DriverT` is called with the TestAutofillClient that
  // owns this factory, a PassKey, and `args`.
  //
  // This and CreateAndActivate() are the only ways of creating a
  // TestAutofillDriver.
  template <typename DriverT,
            typename ManagerT = TestBrowserAutofillManager,
            typename... Args>
    requires(std::derived_from<DriverT, TestAutofillDriver> &&
             std::derived_from<ManagerT, AutofillManager>)
  DriverT& Create(CreateParams params = {}, Args&&... args) {
    // Constructors of `DriverT` may call functions that validate the frame
    // tree.
    ScopedValidationSuppressor suppressor(*this);
    auto driver = std::make_unique<DriverT>(
        &client(), base::PassKey<TestAutofillDriverFactory>(),
        std::forward<Args>(args)...);
    ApplyParams(*driver, params);
    driver->set_autofill_manager(std::make_unique<ManagerT>(driver.get()),
                                 /*pass_key=*/{});
    DriverT& raw_driver = *driver;
    Adopt(std::move(driver));
    return raw_driver;
  }

  TestAutofillDriver& Create(CreateParams params = {});

  // Like Create(), but also activates the new driver.
  template <typename DriverT,
            typename ManagerT = TestBrowserAutofillManager,
            typename... Args>
    requires(std::derived_from<DriverT, TestAutofillDriver> &&
             std::derived_from<ManagerT, AutofillManager>)
  DriverT& CreateAndActivate(CreateParams params = {}, Args&&... args) {
    // The new driver may be the first primary main frame.
    ScopedValidationSuppressor suppressor(*this);
    DriverT& driver =
        Create<DriverT, ManagerT>(params, std::forward<Args>(args)...);
    Activate(driver);
    return driver;
  }

  TestAutofillDriver& CreateAndActivate(CreateParams params = {});

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

 private:
  // The TestAutofillClient that owns this factory.
  TestAutofillClient& client();

  // Applies `params` to `driver`.
  static void ApplyParams(TestAutofillDriver& driver, CreateParams params);

  // Takes ownership of `driver` and notifies the observers.
  void Adopt(std::unique_ptr<TestAutofillDriver> driver);

  // Indicates whether this factory owns `driver`.
  bool Owns(const TestAutofillDriver& driver) const;

  bool is_validation_suppressed() const {
    return num_validation_suppressors_ > 0;
  }

  const raw_ref<TestAutofillClientBase> client_;
  std::vector<std::unique_ptr<TestAutofillDriver>> drivers_;
  bool require_primary_main_frame_ = true;
  size_t num_validation_suppressors_ = 0;
  base::WeakPtrFactory<TestAutofillDriverFactory> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_FACTORY_H_
