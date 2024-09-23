// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result_android.h"

#include <optional>

#include "base/json/values_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

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

TEST(PasswordStatusCheckResultAndroidTest, ResultToDict) {
  auto result = std::make_unique<PasswordStatusCheckResultAndroid>(0);
  result->UpdateCompromisedPasswordCount(1);
  EXPECT_THAT(result->GetCompromisedPasswordsCount(), 1);

  // When converting to dict, the values of the password data should be
  // correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  std::optional<int> compromised_password_count =
      dict.FindInt(safety_hub::kSafetyHubCompromiedPasswordOriginsCount);
  EXPECT_EQ(1, compromised_password_count.value());
}

TEST(PasswordStatusCheckResultAndroidTest, ResultIsTrigger) {
  auto result = std::make_unique<PasswordStatusCheckResultAndroid>(0);

  // When there is no leaked password, then notification should not be
  // triggered.
  EXPECT_FALSE(result->IsTriggerForMenuNotification());

  // When there is a leaked password, then notification can be triggered.
  result->UpdateCompromisedPasswordCount(1);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST(PasswordStatusCheckResultAndroidTest, ResultWarrantsNewNotification) {
  auto old_result = std::make_unique<PasswordStatusCheckResultAndroid>(0);
  auto new_result = std::make_unique<PasswordStatusCheckResultAndroid>(0);
  ValidateNotification(new_result, old_result, false);

  // Compromised pwd count is 1 in new, but 0 in old -> warrants
  // notification
  new_result->UpdateCompromisedPasswordCount(1);
  ValidateNotification(new_result, old_result, true, 1);
  // Compromised pwd count is 1 in both new and old -> no notification
  old_result->UpdateCompromisedPasswordCount(1);
  ValidateNotification(new_result, old_result, false);

  // Compromised pwd count is 2 in new, but 1 in old -> warrants
  // notification
  new_result->UpdateCompromisedPasswordCount(2);
  ValidateNotification(new_result, old_result, true, 2);
  // Compromised pwd count is 2 in both new and old -> no notification
  old_result->UpdateCompromisedPasswordCount(2);
  ValidateNotification(new_result, old_result, false);

  // Compromised pwd count is 1 in new, but 2 in old -> no notification
  new_result->UpdateCompromisedPasswordCount(1);
  ValidateNotification(new_result, old_result, false);
}

TEST(PasswordStatusCheckResultAndroidTest, CloneResult) {
  auto result = std::make_unique<PasswordStatusCheckResultAndroid>(0);
  result->UpdateCompromisedPasswordCount(1);

  auto cloned_result = result->Clone();
  EXPECT_EQ(result->GetCompromisedPasswordsCount(),
            static_cast<PasswordStatusCheckResultAndroid*>(cloned_result.get())
                ->GetCompromisedPasswordsCount());
}
