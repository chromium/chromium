// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/mock_plus_address_setting_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using ::testing::Return;

constexpr std::string_view kPlusAddressModalEventHistogram =
    "PlusAddresses.Modal.Events";
constexpr std::string_view kPlusAddressModalWithNoticeEventHistogram =
    "PlusAddresses.ModalWithNotice.Events";

constexpr base::TimeDelta kDuration = base::Milliseconds(3600);

std::string FormatModalDurationMetrics(
    metrics::PlusAddressModalCompletionStatus status,
    bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.ShownDuration"
                   : "PlusAddresses.Modal.$1.ShownDuration",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

std::string FormatRefreshHistogramNameFor(
    metrics::PlusAddressModalCompletionStatus status,
    bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.Refreshes"
                   : "PlusAddresses.Modal.$1.Refreshes",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerAndroidEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PlusAddressCreationControllerAndroidEnabledTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindLambdaForTesting([&](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakePlusAddressService>();
        }));
    // TODO: crbug.com/322279583 - Use FakePlusAddressSettingService from the
    // PlusAddressTestEnvironment instead.
    PlusAddressSettingServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockPlusAddressSettingService>();
        }));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  FakePlusAddressService& plus_address_service() {
    return static_cast<FakePlusAddressService&>(
        *PlusAddressServiceFactory::GetForBrowserContext(browser_context()));
  }

  MockPlusAddressSettingService& plus_address_setting_service() {
    return static_cast<MockPlusAddressSettingService&>(
        *PlusAddressSettingServiceFactory::GetForBrowserContext(
            browser_context()));
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList features_{features::kPlusAddressesEnabled};
};

// Tests that accepting the bottomsheet calls Autofill to fill the plus address
// and records metrics.
TEST_F(PlusAddressCreationControllerAndroidEnabledTest, AcceptCreation) {
  base::test::ScopedFeatureList features_{
      features::kPlusAddressUserOnboardingEnabled};
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  ON_CALL(plus_address_setting_service(), GetHasAcceptedNotice)
      .WillByDefault(Return(true));
  // The setting service is not called if the notice is already accepted.
  EXPECT_CALL(plus_address_setting_service(), SetHasAcceptedNotice).Times(0);

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      0, 1);
}

// Tests that no notice is shown if the onboarding feature is disabled.
TEST_F(PlusAddressCreationControllerAndroidEnabledTest,
       NoNoticeIfOnboardingFeatureIsDisabled) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  ON_CALL(plus_address_setting_service(), GetHasAcceptedNotice)
      .WillByDefault(Return(false));
  EXPECT_CALL(plus_address_setting_service(), SetHasAcceptedNotice).Times(0);

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
}

// Tests that the notice is shown and its acceptance registered if the
// `kPlusAddressUserOnboardingEnabled` feature is enabled and the notice has not
// been accepted before.
TEST_F(PlusAddressCreationControllerAndroidEnabledTest, ShowNoticeAccept) {
  base::test::ScopedFeatureList features_{
      features::kPlusAddressUserOnboardingEnabled};
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  ON_CALL(plus_address_setting_service(), GetHasAcceptedNotice)
      .WillByDefault(Return(false));
  EXPECT_CALL(plus_address_setting_service(), SetHasAcceptedNotice);

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          kPlusAddressModalWithNoticeEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/true),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/true),
      0, 1);
}

// Tests that the notice is shown if the  `kPlusAddressUserOnboardingEnabled`,
// but cancelling the dialog does not call the settings service.
TEST_F(PlusAddressCreationControllerAndroidEnabledTest, ShowNoticeCancel) {
  base::test::ScopedFeatureList features_{
      features::kPlusAddressUserOnboardingEnabled};
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  ON_CALL(plus_address_setting_service(), GetHasAcceptedNotice)
      .WillByDefault(Return(false));
  EXPECT_CALL(plus_address_setting_service(), SetHasAcceptedNotice).Times(0);

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  FastForwardBy(kDuration);
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          kPlusAddressModalWithNoticeEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/true),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/true),
      0, 1);
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, RefreshPlusAddress) {
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
  controller->OnRefreshClicked();
  FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      1, 1);
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, OnConfirmedError) {
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

  plus_address_service().set_should_fail_to_confirm(true);
  FastForwardBy(kDuration);
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
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      0, 1);
}

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, OnReservedError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);
  base::test::TestFuture<const std::string&> future;

  plus_address_service().set_should_fail_to_reserve(true);

  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  FastForwardBy(kDuration);
  controller->OnCanceled();

  // Ensure that plus address can be canceled after erroneous reserve event and
  // metric is recorded.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError,
          /*notice_shown=*/false),
      0, 1);
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
  FastForwardBy(kDuration);
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      0, 1);
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
  plus_address_service().set_is_confirmed(true);
  plus_address_service().set_confirm_callback(confirm_future.GetCallback());

  base::test::TestFuture<const std::string&> autofill_future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://kirubelwashere.example")),
      autofill_future.GetCallback());
  ASSERT_FALSE(autofill_future.IsReady());

  // Confirmation should fill the field, but not call ConfirmPlusAddress.
  FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(autofill_future.IsReady());
  EXPECT_FALSE(confirm_future.IsReady());

  // Verify that the plus address modal is still shown when this happens.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      0, 1);
}
// With the feature disabled, the `KeyedService` is not present; ensure this is
// handled. While this code path should not be called in that case, it is
// validated here for safety.
class PlusAddressCreationControllerAndroidDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    features_.InitAndDisableFeature(features::kPlusAddressesEnabled);
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

}  // namespace
}  // namespace plus_addresses
