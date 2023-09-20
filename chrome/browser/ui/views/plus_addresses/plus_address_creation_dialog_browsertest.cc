// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

namespace plus_addresses {

namespace {
// Used to control the behavior of the controller's `plus_address_service_`
// (though mocking would also be fine). Most importantly, this avoids the
// requirement to mock the identity portions of the `PlusAddressService`.
class MockPlusAddressService : public PlusAddressService {
 public:
  MockPlusAddressService() = default;

  void OfferPlusAddressCreation(const url::Origin& origin,
                                PlusAddressCallback callback) override {
    std::move(callback).Run("plus+plus@plus.plus");
  }

  absl::optional<std::string> GetPrimaryEmail() override {
    return "plus+plus@plus.plus";
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
    return std::make_unique<MockPlusAddressService>();
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
  base::MockOnceCallback<void(const std::string&)> callback;

  // Then, manually re-trigger the UI, while the modal is still open, passing
  // another callback. The second callback should not be run on confirmation in
  // the modal.
  PlusAddressCreationController* controller =
      PlusAddressCreationController::GetOrCreate(
          browser()->tab_strip_model()->GetActiveWebContents());
  controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                            callback.Get());

  EXPECT_CALL(callback, Run).Times(0);
  controller->OnConfirmed();
}

}  // namespace plus_addresses
