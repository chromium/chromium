// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

constexpr char kFakeEmailAddressForCallback[] = "plus+plus@plus.plus";

// Used to control the behavior of the controller's `plus_address_service_`
// (though mocking would also be fine). Most importantly, this avoids the
// requirement to mock the identity portions of the `PlusAddressService`.
class FakePlusAddressService : public PlusAddressService {
 public:
  FakePlusAddressService() = default;

  void OfferPlusAddressCreation(const url::Origin& origin,
                                PlusAddressCallback callback) override {
    std::move(callback).Run(kFakeEmailAddressForCallback);
  }

  absl::optional<std::string> GetPrimaryEmail() override {
    return "plus+primary@plus.plus";
  }
};
}  // namespace

// TODO(crbug.com/1467623): Consolidate android/desktop controllers, and
// presumably switch to the `PlatformBrowserTest` pattern.
class PlusAddressCreationViewAndroidBrowserTest : public AndroidBrowserTest {
 public:
  PlusAddressCreationViewAndroidBrowserTest()
      : override_profile_selections_(
            PlusAddressServiceFactory::GetInstance(),
            PlusAddressServiceFactory::CreateProfileSelections()) {}

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        chrome_test_utils::GetActiveWebContents(this)->GetBrowserContext(),
        base::BindRepeating(&PlusAddressCreationViewAndroidBrowserTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
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

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest, OfferUi) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));
  base::MockOnceCallback<void(const std::string&)> callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")), callback.Get());

  EXPECT_CALL(callback, Run(kFakeEmailAddressForCallback)).Times(1);
  controller->OnConfirmed();
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest,
                       DoubleOfferUi) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation like normal.
  base::MockOnceCallback<void(const std::string&)> callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")), callback.Get());

  // Then, offer creation a second time, without first dismissing the UI.
  base::MockOnceCallback<void(const std::string&)> second_callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      second_callback.Get());

  EXPECT_CALL(callback, Run(kFakeEmailAddressForCallback)).Times(1);
  EXPECT_CALL(second_callback, Run).Times(0);
  controller->OnConfirmed();
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest, Cancel) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation.
  base::MockOnceCallback<void(const std::string&)> callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")), callback.Get());
  // Then, cancel, and ensure that `callback` is not run.
  EXPECT_CALL(callback, Run).Times(0);
  controller->OnCanceled();
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest,
                       CancelThenShowAgain) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation.
  base::MockOnceCallback<void(const std::string&)> callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")), callback.Get());
  // Then, cancel, destroy, and ensure that `callback` is not run.
  EXPECT_CALL(callback, Run).Times(0);
  controller->OnCanceled();
  controller->OnDialogDestroyed();

  // After re-showing, confirmation should run `second_callback`.
  base::MockOnceCallback<void(const std::string&)> second_callback;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      second_callback.Get());
  EXPECT_CALL(second_callback, Run(kFakeEmailAddressForCallback)).Times(1);
  controller->OnConfirmed();
}

}  //  namespace plus_addresses
