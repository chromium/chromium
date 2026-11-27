// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_WITH_TEST_AUTOFILL_CLIENT_DRIVER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_WITH_TEST_AUTOFILL_CLIENT_DRIVER_MANAGER_H_

#include <concepts>
#include <optional>
#include <type_traits>

#include "base/check.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"

namespace autofill {

// The go-to helper for unit tests to have an AutofillClient, AutofillDriver,
// and AutofillManager.
//
// Usage works as follows:
// - Derive from WithTestAutofillClientDriverManager<...>.
//   The optional template arguments can be used to override the default
//   Autofill{Client,Driver,Manager} implementation.
// - Call InitAutofillClient() and DestroyAutofillClient():
//   - The test needs to call InitAutofillClient() after the TaskEnvironment is
//     created.
//   - The test may need to call DestroyAutofillClient() before the
//     TaskEnvironment is destroyed.
//   Typically, that's in the constructor or SetUp() and the destructor or
//   TearDown().
// - Call CreateAndActivateAutofillDriver() to create an AutofillDriver and
//   AutofillManager. The living AutofillDrivers and their AutofillManagers are
//   destroyed implicitly by DestroyAutofillClient().
//   CreateAutofillDriver() creates an inactive driver instead. Since
//   TestAutofillDriverFactory validates the frame tree, it is only suitable for
//   additional drivers (e.g., subframes), not for the first driver; see the
//   function's documentation for the exceptions.
//
// For example:
//
//   class FooTest
//       : public ::testing::Test,
//         public WithTestAutofillClientDriverManager<TestAutofillClient,
//                                                    MockAutofillDriver> {
//    public:
//     void SetUp()    override { InitAutofillClient(); }
//     void TearDown() override { DestroyAutofillClient(); }
//   };
//
//   TEST_F(FooTest, Bar) {
//     CreateAndActivateAutofillDriver();
//     EXPECT_CALL(autofill_driver(), ApplyFormAction);
//     autofill_manager().FillOrPreviewForm(...);
//   }
template <typename Client = TestAutofillClient,
          typename Driver = TestAutofillDriver,
          typename Manager = TestBrowserAutofillManager,
          typename PaymentsClient = std::remove_pointer_t<
              decltype(std::declval<Client>().GetPaymentsAutofillClient())>>
  requires(
      std::derived_from<Client, TestAutofillClient> &&
      std::derived_from<Driver, TestAutofillDriver> &&
      std::derived_from<Manager, AutofillManager> &&
      std::derived_from<PaymentsClient, payments::TestPaymentsAutofillClient>)
class WithTestAutofillClientDriverManager {
 public:
  WithTestAutofillClientDriverManager() = default;

  WithTestAutofillClientDriverManager(
      const WithTestAutofillClientDriverManager&) = delete;

  WithTestAutofillClientDriverManager& operator=(
      const WithTestAutofillClientDriverManager&) = delete;

  ~WithTestAutofillClientDriverManager() {
    if (client_) {
      DestroyAutofillClient();
    }
  }

  // Sets up the AutofillClient. Must be called before any other other member
  // function.
  //
  // Usually called in the test fixture's constructor or SetUp().
  //
  // This method is virtual to allow tests to customize the client after it was
  // initialized but before it is used to construct AutofillDrivers and
  // AutofillManagers. Subclasses overwriting this method should always call the
  // method of the base class.
  virtual void InitAutofillClient() {
    CHECK(!client_) << "AutofillClient has already been initialized.";
    client_.emplace();
  }

  // Destroys the AutofillClient, including the AutofillDrivers created by
  // Create[AndActivate]AutofillDriver() and their owned AutofillManagers.
  //
  // Usually called in the test fixture's destructor or TearUp(). If it is not
  // called explicitly, ~WithTestAutofillClientDriverManager() calls it.
  void DestroyAutofillClient() {
    CHECK(client_) << "AutofillClient has already been destroyed or was never "
                      "initialized.";
    client_.reset();
  }

  // Creates a new AutofillDriver and its owned AutofillManager, leaving it
  // inactive (`AutofillDriver::LifecycleState::kInactive`). Most tests should
  // prefer `CreateAndActivateAutofillDriver()`.
  //
  // TestAutofillDriverFactory validates the frame tree when it creates the new
  // driver. Therefore, this function CHECK-fails if there is no active primary
  // main frame afterwards. In particular, it cannot be used to create the first
  // driver unless
  // - a TestAutofillDriverFactory::ScopedValidationSuppressor is alive, or
  // - TestAutofillDriverFactory::RequirePrimaryMainFrame(false) was called.
  void CreateAutofillDriver() {
    autofill_client()
        .GetAutofillDriverFactory()
        .template Create<Driver, Manager>();
  }

  // Creates a new AutofillDriver and its owned AutofillManager, and activates
  // the newly created driver via `ActivateAutofillDriver()`.
  void CreateAndActivateAutofillDriver() {
    autofill_client()
        .GetAutofillDriverFactory()
        .template CreateAndActivate<Driver, Manager>();
  }

  Client& autofill_client() {
    CHECK(client_) << "AutofillClient has not yet been initialized. Call "
                      "InitAutofillClient() to do so.";
    return *client_;
  }

  PaymentsClient& payments_autofill_client() {
    return static_cast<PaymentsClient&>(
        CHECK_DEREF(autofill_client().GetPaymentsAutofillClient()));
  }

  // See CreateAndActivateAutofillDriver().
  Driver& autofill_driver(size_t index = 0) {
    TestAutofillDriver* driver =
        autofill_client().GetAutofillDriverFactory().driver(index);
    CHECK(driver) << "There are only "
                  << autofill_client().GetAutofillDriverFactory().num_drivers()
                  << ". Call CreateAndActivateAutofillDriver() to create one.";
    autofill_client().GetAutofillDriverFactory().CheckValidFrameTree();
    return static_cast<Driver&>(*driver);
  }

  // See CreateAndActivateAutofillDriver().
  Manager& autofill_manager(size_t index = 0) {
    return static_cast<Manager&>(autofill_driver(index).GetAutofillManager());
  }

  // See AutofillDriver::LifecycleState for documentation.
  void ActivateAutofillDriver(Driver& driver) {
    autofill_client().GetAutofillDriverFactory().Activate(driver);
  }
  void DeactivateAutofillDriver(Driver& driver) {
    autofill_client().GetAutofillDriverFactory().Deactivate(driver);
  }
  void ResetAutofillDriver(Driver& driver) {
    autofill_client().GetAutofillDriverFactory().Reset(driver);
  }
  void DeleteAutofillDriver(Driver& driver) {
    autofill_client().GetAutofillDriverFactory().Delete(driver);
  }
  void DeleteAllAutofillDrivers() {
    autofill_client().GetAutofillDriverFactory().DeleteAll();
  }

 private:
  std::optional<Client> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_WITH_TEST_AUTOFILL_CLIENT_DRIVER_MANAGER_H_
