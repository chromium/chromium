// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::SizeIs;

constexpr char kPlusAddressModalEventHistogram[] = "PlusAddresses.Modal.Events";
constexpr char kPlusAddressModalEventHistogramWithNotice[] =
    "PlusAddresses.ModalWithNotice.Events";

constexpr base::TimeDelta kDuration = base::Milliseconds(2400);

std::string FormatModalDurationMetrics(
    metrics::PlusAddressModalCompletionStatus status) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.Modal.$1.ShownDuration",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

std::string FormatModalWithNoticeDurationMetrics(
    metrics::PlusAddressModalCompletionStatus status) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.ModalWithNotice.$1.ShownDuration",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerDesktopEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PlusAddressCreationControllerDesktopEnabledTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    features_.InitWithFeatures({features::kPlusAddressesEnabled,
                                features::kPlusAddressUserOnboardingEnabled},
                               {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context(),
        base::BindRepeating(&PlusAddressCreationControllerDesktopEnabledTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
    PlusAddressSettingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context(),
        base::BindRepeating(&PlusAddressCreationControllerDesktopEnabledTest::
                                PlusAddressSettingServiceTestFactory,
                            base::Unretained(this)));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  FakePlusAddressService& plus_address_service() {
    return *static_cast<FakePlusAddressService*>(
        PlusAddressServiceFactory::GetForBrowserContext(browser_context()));
  }

  FakePlusAddressSettingService& setting_service() {
    return *static_cast<FakePlusAddressSettingService*>(
        PlusAddressSettingServiceFactory::GetForBrowserContext(
            browser_context()));
  }

  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<FakePlusAddressService>();
  }

  std::unique_ptr<KeyedService> PlusAddressSettingServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<FakePlusAddressSettingService>();
  }

 private:
  // Ensures that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_tester_;
};

// Tests the scenario when the user successfully creates the first plus address.
TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       ConfirmedFirstTimePlusAddressCreation) {
  setting_service().set_has_accepted_notice(false);

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
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kPlusAddressModalEventHistogramWithNotice),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalWithNoticeDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
      kDuration, 1);
  // The pref is set only when the first time onboarding notice is shown.
  EXPECT_EQ(profile()->GetTestingPrefService()->GetTime(
                prefs::kFirstPlusAddressCreationTime),
            base::Time::Now());
  EXPECT_THAT(plus_address_service().get_triggered_survey_type(),
              Optional(hats::SurveyType::kAcceptedFirstTimeCreate));
}

// Tests the scenario when the user declines the first plus address creation
// flow.
TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       FirstTimePlusAddressCreationDeclined) {
  setting_service().set_has_accepted_notice(false);

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
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);

  controller->OnCanceled();

  EXPECT_FALSE(future.IsReady());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kPlusAddressModalEventHistogramWithNotice),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalWithNoticeDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled),
      kDuration, 1);
  // The pref is set only when the first time onboarding notice is shown.
  EXPECT_EQ(profile()->GetTestingPrefService()->GetTime(
                prefs::kFirstPlusAddressCreationTime),
            base::Time());
  // HaTS survey should be shown if the user declined the first time plus
  // address creation flow.
  EXPECT_THAT(plus_address_service().get_triggered_survey_type(),
              Optional(hats::SurveyType::kDeclinedFirstTimeCreate));
}

// Tests the scenario when the user confirms the first plus address creation
// flow, but the `PlusAddressService` fails to confirm the plus address.
TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       FirstTimePlusAddressCreationFailed) {
  setting_service().set_has_accepted_notice(false);

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
  ASSERT_FALSE(future.IsReady());

  plus_address_service().set_should_fail_to_confirm(true);

  task_environment()->FastForwardBy(kDuration);

  controller->OnConfirmed();

  EXPECT_FALSE(future.IsReady());

  // When `ConfirmPlusAddress` fails, `OnCanceled` may be called after
  // `OnConfirmed`.
  controller->OnCanceled();

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kPlusAddressModalEventHistogramWithNotice),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalWithNoticeDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError),
      kDuration, 1);
  // The pref is not set of the first plus address creation flow failed.
  EXPECT_EQ(profile()->GetTestingPrefService()->GetTime(
                prefs::kFirstPlusAddressCreationTime),
            base::Time());
  // Feature perception surveys shown after the first plus address creation
  // flow should not be triggered if the plus address wasn't confirmed.
  EXPECT_EQ(plus_address_service().get_triggered_survey_type(), std::nullopt);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, DirectCallback) {
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
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);
  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
      kDuration, 1);
  // The pref is not set after the first time onboarding notice has been already
  // shown.
  EXPECT_EQ(profile()->GetTestingPrefService()->GetTime(
                prefs::kFirstPlusAddressCreationTime),
            base::Time());
  EXPECT_EQ(plus_address_service().get_triggered_survey_type(), std::nullopt);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, OnConfirmedError) {
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
  ASSERT_FALSE(future.IsReady());

  plus_address_service().set_should_fail_to_confirm(true);

  task_environment()->FastForwardBy(kDuration);

  controller->OnConfirmed();

  EXPECT_FALSE(future.IsReady());
  // When `ConfirmPlusAddress` fails, `OnCanceled` may be called after
  // `OnConfirmed`.
  controller->OnCanceled();
  // Ensure that plus address can be canceled after erroneous confirm event and
  // metric is recorded.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError),
      kDuration, 1);
}

// Tests that the user can retry creating a plus address after the previous
// attempt fails. Verifies that the correct metrics are logged in this case.
TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       ConfirmAfterCreateError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;

  controller->OfferCreation(
      url::Origin::Create(GURL("https://timofeywashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  plus_address_service().set_should_fail_to_confirm(true);

  task_environment()->FastForwardBy(kDuration);

  controller->OnConfirmed();
  EXPECT_FALSE(future.IsReady());

  plus_address_service().set_should_fail_to_confirm(false);
  task_environment()->FastForwardBy(kDuration);

  controller->OnConfirmed();
  EXPECT_TRUE(future.IsReady());

  // Ensure that plus address can be confirmed after a confirm error is shown.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 2)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
      2 * kDuration, 1);
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, OnReservedError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  plus_address_service().set_should_fail_to_reserve(true);

  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);

  controller->OnCanceled();
  // Ensure that plus address can be canceled after erroneous reserve event and
  // metric is recorded.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError),
      kDuration, 1);
}

// Tests that the user can retry confirming a plus address after the previous
// attempt to reserve it failed. Verifies that the correct metrics are logged
// in this case.
TEST_F(PlusAddressCreationControllerDesktopEnabledTest,
       ConfirmAfterReserveError) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::test::TestFuture<const std::string&> future;
  plus_address_service().set_should_fail_to_reserve(true);

  controller->OfferCreation(
      url::Origin::Create(GURL("https://timofeywashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);

  controller->set_suppress_ui_for_testing(false);
  controller->OnRefreshClicked();
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);

  controller->OnConfirmed();
  ASSERT_TRUE(future.IsReady());
  // Ensure that plus address can be confirmed after an error is shown and then
  // the plus address is successfully reserved.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
      2 * kDuration, 1);
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

  base::test::TestFuture<const std::string&> autofill_future;
  base::test::TestFuture<const PlusProfileOrError&> confirm_future;

  // Make Reserve() return kFakePlusAddress as an already-confirmed address.
  plus_address_service().set_is_confirmed(true);
  plus_address_service().set_confirm_callback(confirm_future.GetCallback());

  controller->OfferCreation(
      url::Origin::Create(GURL("https://kirubelwashere.example")),
      autofill_future.GetCallback());
  ASSERT_FALSE(autofill_future.IsReady());

  task_environment()->FastForwardBy(kDuration);
  // Confirmation should fill the field, but not call ConfirmPlusAddress.
  controller->OnConfirmed();
  EXPECT_TRUE(autofill_future.IsReady());
  EXPECT_FALSE(confirm_future.IsReady());

  // Verify that the plus address modal is still shown.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
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

  base::test::TestFuture<const std::string&> future;
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());

  task_environment()->FastForwardBy(kDuration);
  controller->OnCanceled();
  EXPECT_FALSE(future.IsReady());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester().ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalCanceled),
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

}  // namespace
}  // namespace plus_addresses
