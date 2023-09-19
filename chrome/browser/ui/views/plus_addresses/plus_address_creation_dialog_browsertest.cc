// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/plus_addresses/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

namespace plus_addresses {

class PlusAddressCreationDialogTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    PlusAddressCreationController* controller =
        PlusAddressCreationController::GetOrCreate(
            browser()->tab_strip_model()->GetActiveWebContents());
    controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                              base::DoNothing());
  }

 protected:
  base::test::ScopedFeatureList features_{kFeature};
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
