// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

namespace plus_addresses {

namespace {
// Used to control the behavior of the controller's `plus_address_service_`
// (though mocking would also be fine). Most importantly, this avoids the
// requirement to mock the identity portions of the `PlusAddressService`.
class FakePlusAddressService : public PlusAddressService {
 public:
  FakePlusAddressService() = default;

  void OfferPlusAddressCreation(const url::Origin& origin,
                                PlusAddressCallback on_completed) override {
    std::move(on_completed).Run(plus_address_);
  }

  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override {
    std::move(on_completed)
        .Run(PlusProfile({.facet = facet_,
                          .plus_address = plus_address_,
                          .is_confirmed = false}));
  }

  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed) override {
    std::move(on_completed)
        .Run(PlusProfile({.facet = facet_,
                          .plus_address = plus_address_,
                          .is_confirmed = true}));
  }

  std::string plus_address_ = "plus+plus@plus.plus";
  std::string facet_ = "facet.bar";

  absl::optional<std::string> GetPrimaryEmail() override {
    return "plus+primary@plus.plus";
  }
};
}  // namespace

class PlusAddressCreationDialogTest : public DialogBrowserTest {
 public:
  PlusAddressCreationDialogTest()
      : override_profile_selections_(
            PlusAddressServiceFactory::GetInstance(),
            PlusAddressServiceFactory::CreateProfileSelections()) {}

  void ShowUi(const std::string& name) override {
    // Ensure the `PlusAddressService` will behave as needed. As this is
    // checking the dialog, the identity service integration, etc. is less
    // critical. This setup done here to ensure `GetActiveWebContents()` is
    // ready.
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetBrowserContext(),
        base::BindRepeating(
            &PlusAddressCreationDialogTest::PlusAddressServiceTestFactory,
            base::Unretained(this)));

    PlusAddressCreationController* controller =
        PlusAddressCreationController::GetOrCreate(
            browser()->tab_strip_model()->GetActiveWebContents());
    controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                              base::DoNothing());
  }

  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<FakePlusAddressService>();
  }

 protected:
  base::test::ScopedFeatureList features_{kFeature};
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
};

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, BasicUiVerify) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, DoubleInit) {
  // First, show the UI normally.
  ShowUi(std::string());
  base::test::TestFuture<const std::string&> future;

  // Then, manually re-trigger the UI, while the modal is still open, passing
  // another callback. The second callback should not be run on confirmation in
  // the modal.
  PlusAddressCreationController* controller =
      PlusAddressCreationController::GetOrCreate(
          browser()->tab_strip_model()->GetActiveWebContents());
  controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                            future.GetCallback());

  controller->OnConfirmed();
  EXPECT_FALSE(future.IsReady());
}

}  // namespace plus_addresses
