// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
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

  std::string plus_address_ = kFakeEmailAddressForCallback;
  std::string facet_ = "facet.bar";

  std::optional<std::string> GetPrimaryEmail() override {
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
  base::test::ScopedFeatureList features_{features::kFeature};
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
};

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest, OfferUi) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kFakeEmailAddressForCallback);
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest,
                       DoubleOfferUi) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation like normal.
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      future.GetCallback());

  // Then, offer creation a second time, without first dismissing the UI.
  base::test::TestFuture<const std::string&> second_future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      second_future.GetCallback());

  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kFakeEmailAddressForCallback);
  EXPECT_FALSE(second_future.IsReady());
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest, Cancel) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation.
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      future.GetCallback());
  // Then, cancel, and ensure that `future.GetCallback()` is not run.
  EXPECT_FALSE(future.IsReady());
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest,
                       CancelThenShowAgain) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          chrome_test_utils::GetActiveWebContents(this));

  // First, offer creation.
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      future.GetCallback());
  // Then, cancel, destroy, and ensure that `future.GetCallback()` is not run.
  controller->OnCanceled();
  controller->OnDialogDestroyed();
  EXPECT_FALSE(future.IsReady());

  // After re-showing, confirmation should run `second_future.GetCallback()`.
  base::test::TestFuture<const std::string&> second_future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      second_future.GetCallback());
  controller->OnConfirmed();
  EXPECT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get(), kFakeEmailAddressForCallback);
}

// Ensure that closing the web contents with the plus_address creation UI open
// doesn't cause issues, and doesn't incorrectly invoke the autofill callback.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationViewAndroidBrowserTest,
                       CloseWebContents) {
  content::WebContents* active_web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  PlusAddressCreationControllerAndroid::CreateForWebContents(
      active_web_contents);
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(
          active_web_contents);
  base::test::TestFuture<const std::string&> future;
  // First, offer creation.
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.com")),
      future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  // Next, close the web contents. The view and controller will be destroyed.
  active_web_contents->Close();
  // Expect no autofill callback.
  EXPECT_FALSE(future.IsReady());
}

}  //  namespace plus_addresses
