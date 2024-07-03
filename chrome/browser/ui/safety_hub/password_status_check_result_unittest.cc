// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result.h"

#include "base/json/values_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kOrigin1[] = "https://example1.com/";
constexpr char kOrigin2[] = "https://example2.com/";
constexpr char kUsername1[] = "user1";
constexpr char kUsername2[] = "user2";

void ValidateNotification(const auto& new_result,
                          const auto& old_result,
                          bool is_warrant,
                          int count = -1) {
  EXPECT_EQ(is_warrant,
            new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));

  if (count != -1) {
    EXPECT_EQ(
        new_result->GetNotificationString(),
        l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION,
            count));
  }
}

}  // namespace

TEST(PasswordStatusCheckResultTest, ResultToDict) {
  auto result = std::make_unique<PasswordStatusCheckResult>();
  result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  EXPECT_THAT(result->GetCompromisedPasswords(),
              testing::ElementsAre(PasswordPair(kOrigin1, kUsername1)));

  // When converting to dict, the values of the password data should be
  // correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* compromised_password_list =
      dict.FindList(safety_hub::kSafetyHubPasswordCheckOriginsKey);
  EXPECT_EQ(1U, compromised_password_list->size());
  base::Value::Dict& password_data =
      compromised_password_list->front().GetDict();
  EXPECT_EQ(kOrigin1, password_data.Find(safety_hub::kOrigin)->GetString());
  EXPECT_EQ(kUsername1, password_data.Find(safety_hub::kUsername)->GetString());
}

TEST(PasswordStatusCheckResultTest, ResultIsTrigger) {
  auto result = std::make_unique<PasswordStatusCheckResult>();

  // When there is no leaked password, then notification should not be
  // triggered.
  EXPECT_FALSE(result->IsTriggerForMenuNotification());

  // When there is a leaked password, then notification can be triggered.
  result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST(PasswordStatusCheckResultTest, ResultWarrantsNewNotification) {
  auto old_result = std::make_unique<PasswordStatusCheckResult>();
  auto new_result = std::make_unique<PasswordStatusCheckResult>();
  ValidateNotification(new_result, old_result, false);

  // kOrigin1 and kUsername1 is set in new, but not in old -> warrants
  // notification
  new_result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  ValidateNotification(new_result, old_result, true, 1);
  // kOrigin1 and kUsername1  is in both new and old -> no notification
  old_result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  ValidateNotification(new_result, old_result, false, 1);
  // kOrigin1 and kUsername1 is set again in new -> no notification
  new_result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  ValidateNotification(new_result, old_result, false, 1);

  // kOrigin2 and kUsername1 is added in new, but not in old -> warrants
  // notification
  new_result->AddToCompromisedPasswords(kOrigin2, kUsername1);
  ValidateNotification(new_result, old_result, true, 2);
  // kOrigin2 and kUsername1 is also in both new and old -> no notification
  old_result->AddToCompromisedPasswords(kOrigin2, kUsername1);
  ValidateNotification(new_result, old_result, false, 2);

  // kOrigin2 and kUsername2 is added in new, but not in old -> warrants
  // notification
  new_result->AddToCompromisedPasswords(kOrigin2, kUsername2);
  ValidateNotification(new_result, old_result, true, 3);
  // kOrigin2 and kUsername2 is also in both new and old -> no notification
  old_result->AddToCompromisedPasswords(kOrigin2, kUsername2);
  ValidateNotification(new_result, old_result, false, 3);
}

TEST(PasswordStatusCheckResultTest,
     ResultWarrantsNewNotification_SameOriginDifferentUser) {
  auto old_result = std::make_unique<PasswordStatusCheckResult>();
  auto new_result = std::make_unique<PasswordStatusCheckResult>();
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));

  // kOrigin1 and kUsername1 is set in new, but not in old -> warrants
  // notification
  new_result->AddToCompromisedPasswords(kOrigin1, kUsername1);
  ValidateNotification(new_result, old_result, true, 1);

  // kOrigin1 and kUsername2  is in old, but not in new -> warrants notification
  old_result->AddToCompromisedPasswords(kOrigin1, kUsername2);
  ValidateNotification(new_result, old_result, true, 1);
}

TEST(PasswordStatusCheckResultTest, CloneResult) {
  auto result = std::make_unique<PasswordStatusCheckResult>();
  result->AddToCompromisedPasswords(kOrigin1, kUsername1);

  auto cloned_result = result->Clone();
  EXPECT_EQ(result->GetCompromisedPasswords(),
            static_cast<PasswordStatusCheckResult*>(cloned_result.get())
                ->GetCompromisedPasswords());
}
