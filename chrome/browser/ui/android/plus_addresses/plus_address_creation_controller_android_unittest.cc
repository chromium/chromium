// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

class PlusAddressCreationControllerAndroidTest : public ::testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

// With the feature disabled, the `KeyedService` is not present; ensure this is
// handled. While this code path should not be called in that case, it is
// validated here for safety.
TEST_F(PlusAddressCreationControllerAndroidTest, ConfirmedNullService) {
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
