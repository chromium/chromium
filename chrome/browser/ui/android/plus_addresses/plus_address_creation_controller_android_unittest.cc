// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(crbug.com/1467623): Consolidate this and the desktop version. Splitting
// them was a mechanism to reduce dependencies during implementation, but isn't
// the ideal long-term state.
namespace plus_addresses {

namespace {

constexpr char kFakePlusAddress[] = "plus+remote@plus.plus";

constexpr char kPlusAddressModalEventHistogram[] =
    "Autofill.PlusAddresses.Modal.Events";

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
                          .plus_address = kFakePlusAddress,
                          .is_confirmed = is_confirmed_}));
  }

  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed) override {
    is_confirmed_ = true;
    PlusProfile profile({.facet = facet_,
                         .plus_address = plus_address,
                         .is_confirmed = is_confirmed_});
    if (on_confirmed.has_value()) {
      std::move(on_confirmed.value()).Run(profile);
      on_confirmed.reset();
      return;
    }
    std::move(on_completed).Run(profile);
  }

  // Used to test scenarios where Reserve returns a confirmed PlusProfile.
  void set_is_confirmed(bool confirmed) { is_confirmed_ = confirmed; }

  void set_confirm_callback(PlusAddressRequestCallback callback) {
    on_confirmed = std::move(callback);
  }

  absl::optional<PlusAddressRequestCallback> on_confirmed;
  std::string facet_ = "facet.bar";
  bool is_confirmed_ = false;
  std::string primary_email_address_ = "primary@plus.plus";

  absl::optional<std::string> GetPrimaryEmail() override {
    // Ensure the value is present without requiring identity setup.
    return primary_email_address_;
  }
};
}  // namespace

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerAndroidEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PlusAddressCreationControllerAndroidEnabledTest()
      : override_profile_selections_(
            PlusAddressServiceFactory::GetInstance(),
            PlusAddressServiceFactory::CreateProfileSelections()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context(),
        base::BindRepeating(&PlusAddressCreationControllerAndroidEnabledTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
  }
  void TearDown() override {
    fake_plus_address_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }
  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    std::unique_ptr<FakePlusAddressService> unique_service =
        std::make_unique<FakePlusAddressService>();
    fake_plus_address_service_ = unique_service.get();
    return unique_service;
  }

 protected:
  base::test::ScopedFeatureList features_{kFeature};
  // Ensures that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
  base::HistogramTester histogram_tester_;
  raw_ptr<FakePlusAddressService> fake_plus_address_service_;
};

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, DirectCallback) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest,
       StoredPlusProfileClearedOnDialogDestroyed) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  EXPECT_FALSE(controller->get_plus_profile_for_testing().has_value());
  // Offering creation calls Reserve() and sets the profile.
  controller->OfferCreation(url::Origin::Create(GURL("https://foo.example")),
                            base::DoNothing());
  // Destroying the modal clears the profile
  EXPECT_TRUE(controller->get_plus_profile_for_testing().has_value());
  controller->OnDialogDestroyed();
  EXPECT_FALSE(controller->get_plus_profile_for_testing().has_value());
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, ModalCanceled) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest,
       ReserveGivesConfirmedAddress_DoesntCallService) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  // Setup fake service behavior.
  base::test::TestFuture<const PlusProfileOrError&> confirm_future;
  fake_plus_address_service_->set_is_confirmed(true);
  fake_plus_address_service_->set_confirm_callback(
      confirm_future.GetCallback());

  base::test::TestFuture<const std::string&> autofill_future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://kirubelwashere.example")),
      autofill_future.GetCallback());
  ASSERT_FALSE(autofill_future.IsReady());

  // Confirmation should fill the field, but not call ConfirmPlusAddress.
  controller->OnConfirmed();
  EXPECT_TRUE(autofill_future.IsReady());
  EXPECT_FALSE(confirm_future.IsReady());

  // Verify that the plus address modal is still shown when this happens.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
}
// With the feature disabled, the `KeyedService` is not present; ensure this is
// handled. While this code path should not be called in that case, it is
// validated here for safety.
class PlusAddressCreationControllerAndroidDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    features_.InitAndDisableFeature(kFeature);
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(
            [](content::BrowserContext* profile)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PlusAddressCreationControllerAndroidDisabledTest, ConfirmedNullService) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                            future.GetCallback());
  EXPECT_CHECK_DEATH(controller->OnConfirmed());
  EXPECT_FALSE(future.IsReady());
}

}  // namespace plus_addresses
