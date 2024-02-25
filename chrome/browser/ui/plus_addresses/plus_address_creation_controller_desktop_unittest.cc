// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

constexpr char kPlusAddressModalEventHistogram[] =
    "Autofill.PlusAddresses.Modal.Events";

constexpr char kFakePlusAddress[] = "plus+remote@plus.plus";
constexpr base::TimeDelta kDuration = base::Milliseconds(2400);
constexpr base::Time kStartTime = base::Time::FromSecondsSinceUnixEpoch(1);

std::string FormatModalDurationMetrics(
    PlusAddressMetrics::PlusAddressModalCompletionStatus status) {
  return base::ReplaceStringPlaceholders(
      "Autofill.PlusAddresses.Modal.$1.ShownDuration",
      {PlusAddressMetrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

// Used to control the behavior of the controller's `plus_address_service_`
// (though mocking would also be fine). Most importantly, this avoids the
// requirement to mock the identity portions of the `PlusAddressService`.
class FakePlusAddressService : public PlusAddressService {
 public:
  FakePlusAddressService() = default;

  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override {
    if (should_fail_to_reserve_) {
      std::move(on_completed)
          .Run(base::unexpected(PlusAddressRequestError(
              PlusAddressRequestErrorType::kNetworkError)));
      return;
    }
    std::move(on_completed)
        .Run(PlusProfile({.facet = facet_,
                          .plus_address = kFakePlusAddress,
                          .is_confirmed = is_confirmed_}));
  }

  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed) override {
    if (should_fail_to_confirm_) {
      std::move(on_completed)
          .Run(base::unexpected(PlusAddressRequestError(
              PlusAddressRequestErrorType::kNetworkError)));
      return;
    }
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

  // Used to test scenarios where error occurs on `ConfirmPlusAddress`.
  void set_should_fail_to_confirm(bool status) {
    should_fail_to_confirm_ = status;
  }
  // Used to test scenarios where error occurs on `ReservePlusAddress`.
  void set_should_fail_to_reserve(bool status) {
    should_fail_to_reserve_ = status;
  }

  std::optional<PlusAddressRequestCallback> on_confirmed;
  std::string facet_ = "facet.bar";
  bool is_confirmed_ = false;
  bool should_fail_to_confirm_ = false;
  bool should_fail_to_reserve_ = false;

  std::optional<std::string> GetPrimaryEmail() override {
    // Ensure the value is present without requiring identity setup.
    return "plus+primary@plus.plus";
  }
};

}  // namespace

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerDesktopEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PlusAddressCreationControllerDesktopEnabledTest()
      : override_profile_selections_(
            PlusAddressServiceFactory::GetInstance(),
            PlusAddressServiceFactory::CreateProfileSelections()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context(),
        base::BindRepeating(&PlusAddressCreationControllerDesktopEnabledTest::
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
  base::test::ScopedFeatureList features_{features::kFeature};
  // Ensures that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
  base::HistogramTester histogram_tester_;
  raw_ptr<FakePlusAddressService> fake_plus_address_service_;
  base::SimpleTestClock test_clock_;
};

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, DirectCallback) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  controller->SetClockForTesting(&test_clock_);

  base::test::TestFuture<const std::string&> future;

  test_clock_.SetNow(kStartTime);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  test_clock_.SetNow(kStartTime + kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kModalConfirmed),
      kDuration, 1);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, OnConfirmedError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  controller->SetClockForTesting(&test_clock_);

  base::test::TestFuture<const std::string&> future;

  test_clock_.SetNow(kStartTime);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  fake_plus_address_service_->set_should_fail_to_confirm(true);

  test_clock_.SetNow(kStartTime + kDuration);

  controller->OnConfirmed();

  EXPECT_FALSE(future.IsReady());
  // When `ConfirmPlusAddress` fails, `OnCanceled` may be called after
  // `OnConfirmed`.
  controller->OnCanceled();
  // Ensure that plus address can be canceled after erroneous confirm event and
  // metric is recorded.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kConfirmPlusAddressError),
      kDuration, 1);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, OnReservedError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  controller->SetClockForTesting(&test_clock_);

  base::test::TestFuture<const std::string&> future;
  fake_plus_address_service_->set_should_fail_to_reserve(true);

  test_clock_.SetNow(kStartTime);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  test_clock_.SetNow(kStartTime + kDuration);

  controller->OnCanceled();
  // Ensure that plus address can be canceled after erroneous reserve event and
  // metric is recorded.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kReservePlusAddressError),
      kDuration, 1);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       ReserveGivesConfirmedAddress_DoesntConfirmAgain) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  controller->SetClockForTesting(&test_clock_);

  base::test::TestFuture<const std::string&> autofill_future;
  base::test::TestFuture<const PlusProfileOrError&> confirm_future;

  // Make Reserve() return kFakePlusAddress as an already-confirmed address.
  fake_plus_address_service_->set_is_confirmed(true);
  fake_plus_address_service_->set_confirm_callback(
      confirm_future.GetCallback());

  test_clock_.SetNow(kStartTime);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://kirubelwashere.example")),
      autofill_future.GetCallback());
  ASSERT_FALSE(autofill_future.IsReady());

  test_clock_.SetNow(kStartTime + kDuration);
  // Confirmation should fill the field, but not call ConfirmPlusAddress.
  controller->OnConfirmed();
  EXPECT_TRUE(autofill_future.IsReady());
  EXPECT_FALSE(confirm_future.IsReady());

  // Verify that the plus address modal is still shown.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kModalConfirmed),
      kDuration, 1);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       StoredPlusProfileClearedOnDialogDestroyed) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  EXPECT_FALSE(controller->get_plus_profile_for_testing().has_value());
  // Offering creation calls Reserve() and sets the profile.
  controller->OfferCreation(url::Origin::Create(GURL("https://foo.example")),
                            base::DoNothing());
  EXPECT_TRUE(controller->get_plus_profile_for_testing().has_value());
  // Destroying the dialog clears the profile.
  controller->OnDialogDestroyed();
  EXPECT_FALSE(controller->get_plus_profile_for_testing().has_value());
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, ModalCanceled) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  controller->SetClockForTesting(&test_clock_);

  base::test::TestFuture<const std::string&> future;
  test_clock_.SetNow(kStartTime);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());

  test_clock_.SetNow(kStartTime + kDuration);
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressMetrics::PlusAddressModalCompletionStatus::kModalCanceled),
      kDuration, 1);
}

// With the feature disabled, the `KeyedService` is not present; ensure this is
// handled. While this code path should not be called in that case, it is
// validated here for safety.
class PlusAddressCreationControllerDesktopDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(
            [](content::BrowserContext* profile)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
  }
};

TEST_F(PlusAddressCreationControllerDesktopDisabledTest, NullService) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}
}  // namespace plus_addresses
