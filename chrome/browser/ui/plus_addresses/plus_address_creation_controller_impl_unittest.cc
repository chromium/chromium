// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_impl.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

// Testing very basic functionality for now. As UI complexity increases, this
// class will grow and mutate.
class PlusAddressCreationControllerImplEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_.MakeAccountAvailable("plus@plus.plus",
                                            {signin::ConsentLevel::kSignin});
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(&PlusAddressCreationControllerImplEnabledTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
  }

 protected:
  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<PlusAddressService>(
        identity_test_env_.identity_manager());
  }
  base::test::ScopedFeatureList features_{kFeature};
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(PlusAddressCreationControllerImplEnabledTest, DirectCallback) {
  std::unique_ptr<content::WebContents> web_contents =
      ChromeRenderViewHostTestHarness::CreateTestWebContents();

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
class PlusAddressCreationControllerImplDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_.MakeAccountAvailable("plus@plus.plus",
                                            {signin::ConsentLevel::kSignin});
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(
            [](content::BrowserContext* profile)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
  }

 protected:
  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<PlusAddressService>(
        identity_test_env_.identity_manager());
  }
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(PlusAddressCreationControllerImplDisabledTest, NullService) {
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
