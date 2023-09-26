// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
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
    // Ensure the value is present without requiring identity setup.
    return "plus+plus@plus.plus";
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

  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<MockPlusAddressService>();
  }

 protected:
  base::test::ScopedFeatureList features_{kFeature};
  // Ensures that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, DirectCallback) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(1);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
  controller->OnConfirmed();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
}

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, ModalCanceled) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

  PlusAddressCreationControllerDesktop::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerDesktop* controller =
      PlusAddressCreationControllerDesktop::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(0);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
  controller->OnCanceled();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
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

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(0);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
  controller->OnConfirmed();
}
}  // namespace plus_addresses
