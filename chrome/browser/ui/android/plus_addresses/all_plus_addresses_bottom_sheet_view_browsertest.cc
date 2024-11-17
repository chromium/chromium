// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_controller.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using ::testing::Eq;
using ::testing::Optional;

class AllPlusAddressesBottomSheetViewBrowserTest : public AndroidBrowserTest {
 public:
  AllPlusAddressesBottomSheetViewBrowserTest() {}

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        chrome_test_utils::GetActiveWebContents(this)->GetBrowserContext(),
        base::BindRepeating(&AllPlusAddressesBottomSheetViewBrowserTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
    controller_ = std::make_unique<AllPlusAddressesBottomSheetController>(
        chrome_test_utils::GetActiveWebContents(this));
  }

  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<FakePlusAddressService>();
  }

 protected:
  AllPlusAddressesBottomSheetController& controller() { return *controller_; }

  Profile* profile() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }

 private:
  base::test::ScopedFeatureList features_{features::kPlusAddressesEnabled};
  std::unique_ptr<AllPlusAddressesBottomSheetController> controller_;
};

IN_PROC_BROWSER_TEST_F(AllPlusAddressesBottomSheetViewBrowserTest,
                       OfferUi_SelectPlusAddress) {
  base::MockCallback<
      AllPlusAddressesBottomSheetController::SelectPlusAddressCallback>
      callback;
  controller().Show(callback.Get());

  const std::string plus_address = "example@gmail.com";
  EXPECT_CALL(callback, Run(Optional(plus_address)));
  controller().OnPlusAddressSelected(plus_address);
}

IN_PROC_BROWSER_TEST_F(AllPlusAddressesBottomSheetViewBrowserTest,
                       OfferUi_DismissBottomSheet) {
  base::MockCallback<
      AllPlusAddressesBottomSheetController::SelectPlusAddressCallback>
      callback;
  controller().Show(callback.Get());

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  controller().OnBottomSheetDismissed();
}

}  // namespace
}  // namespace plus_addresses
