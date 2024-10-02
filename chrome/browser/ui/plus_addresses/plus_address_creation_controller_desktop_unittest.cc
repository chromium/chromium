// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

constexpr char kPlusAddressModalEventHistogram[] = "PlusAddresses.Modal.Events";

constexpr base::TimeDelta kDuration = base::Milliseconds(2400);

std::string FormatModalDurationMetrics(
    metrics::PlusAddressModalCompletionStatus status) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.Modal.$1.ShownDuration",
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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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
    auto unique_service = std::make_unique<FakePlusAddressService>();
    fake_plus_address_service_ = unique_service.get();
    return unique_service;
  }

 protected:
  // Ensures that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  base::test::ScopedFeatureList features_{features::kPlusAddressesEnabled};
  base::HistogramTester histogram_tester_;
  raw_ptr<FakePlusAddressService> fake_plus_address_service_ = nullptr;
};

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
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed),
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

  base::test::TestFuture<const std::string&> future;

  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  fake_plus_address_service_->set_should_fail_to_confirm(true);

  task_environment()->FastForwardBy(kDuration);

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
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError),
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

  base::test::TestFuture<const std::string&> future;
  fake_plus_address_service_->set_should_fail_to_reserve(true);

  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")),
      future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kDuration);

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
          metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError),
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

  base::test::TestFuture<const std::string&> autofill_future;
  base::test::TestFuture<const PlusProfileOrError&> confirm_future;

  // Make Reserve() return kFakePlusAddress as an already-confirmed address.
  fake_plus_address_service_->set_is_confirmed(true);
  fake_plus_address_service_->set_confirm_callback(
      confirm_future.GetCallback());

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
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
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
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(metrics::PlusAddressModalEvent::kModalShown, 1),
          base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
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
