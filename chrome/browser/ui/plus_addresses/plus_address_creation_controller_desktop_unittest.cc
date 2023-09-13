// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {
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
};
}  // namespace

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerDesktopEnabledTest : public testing::Test {
 public:
  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<MockPlusAddressService>();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList features_{kFeature};
};

TEST_F(PlusAddressCreationControllerDesktopEnabledTest, DirectCallback) {
  // Ensure that the feature is known to be enabled, such that
  // `PlusAddressServiceFactory` doesn't bail early with a null return.
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      overide_profile_selections(
          PlusAddressServiceFactory::GetInstance(),
          PlusAddressServiceFactory::CreateProfileSelections());

  TestingProfile test_profile;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&test_profile, nullptr);
  PlusAddressServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &test_profile,
      base::BindRepeating(&PlusAddressCreationControllerDesktopEnabledTest::
                              PlusAddressServiceTestFactory,
                          base::Unretained(this)));

  PlusAddressCreationController* controller =
      PlusAddressCreationController::GetOrCreate(web_contents.get());

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(1);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
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

  PlusAddressCreationController* controller =
      PlusAddressCreationController::GetOrCreate(web_contents.get());
  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(0);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
}
}  // namespace plus_addresses
