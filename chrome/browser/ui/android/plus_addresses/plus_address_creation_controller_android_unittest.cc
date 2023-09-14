// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/plus_addresses/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/1467623): Consolidate this and the desktop version. Splitting
// them was a mechanism to reduce dependencies during implementation, but isn't
// the ideal long-term state.
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
class PlusAddressCreationControllerAndroidEnabledTest : public testing::Test {
 public:
  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<MockPlusAddressService>();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_{kFeature};
};

TEST_F(PlusAddressCreationControllerAndroidEnabledTest, DirectCallback) {
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
      base::BindRepeating(&PlusAddressCreationControllerAndroidEnabledTest::
                              PlusAddressServiceTestFactory,
                          base::Unretained(this)));

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(1);
  controller->OfferCreation(
      url::Origin::Create(GURL("https://mattwashere.example")), callback.Get());
  controller->OnConfirmed();
}

class PlusAddressCreationControllerAndroidDisabledTest
    : public ::testing::Test {
 public:
  void SetUp() override { features_.InitAndDisableFeature(kFeature); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

// With the feature disabled, the `KeyedService` is not present; ensure this is
// handled. While this code path should not be called in that case, it is
// validated here for safety.
TEST_F(PlusAddressCreationControllerAndroidDisabledTest, ConfirmedNullService) {
  TestingProfile profile;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile, nullptr);

  PlusAddressCreationControllerAndroid::CreateForWebContents(
      web_contents.get());
  PlusAddressCreationControllerAndroid* controller =
      PlusAddressCreationControllerAndroid::FromWebContents(web_contents.get());
  controller->set_suppress_ui_for_testing(true);

  base::MockOnceCallback<void(const std::string&)> callback;
  controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                            callback.Get());

  PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
      &profile, base::BindRepeating(
                    [](content::BrowserContext* profile)
                        -> std::unique_ptr<KeyedService> { return nullptr; }));
  EXPECT_CALL(callback, Run).Times(0);

  controller->OnConfirmed();
}

}  // namespace plus_addresses
