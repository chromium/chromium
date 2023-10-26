// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class SafeBrowsingResultTest : public testing::Test {
 public:
  SafeBrowsingResultTest()
      : prefs_(std::make_unique<TestingPrefServiceSimple>()) {}
  ~SafeBrowsingResultTest() override = default;

 protected:
  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(SafeBrowsingResultTest, ResultToFromDictAndClone) {
  // When creating a result where the status is set to disabled, which is a
  // trigger for a menu notification, it should remain as such even after it was
  // turned into Dict.
  auto result = std::make_unique<SafetyHubSafeBrowsingResult>(
      SafeBrowsingState::kDisabledByUser);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());

  auto new_result =
      std::make_unique<SafetyHubSafeBrowsingResult>(result->ToDictValue());
  EXPECT_TRUE(new_result->IsTriggerForMenuNotification());

  auto cloned_result = new_result->Clone();
  EXPECT_TRUE(cloned_result->IsTriggerForMenuNotification());
}

TEST_F(SafeBrowsingResultTest, ResultIsTrigger) {
  // Only a disabled status should result in a menu notification.
  for (int i = 0; i <= int(SafeBrowsingState::kMaxValue); i++) {
    auto status = static_cast<SafeBrowsingState>(i);
    auto result = std::make_unique<SafetyHubSafeBrowsingResult>(status);
    if (status == SafeBrowsingState::kDisabledByUser) {
      EXPECT_TRUE(result->IsTriggerForMenuNotification());
      continue;
    }
    EXPECT_FALSE(result->IsTriggerForMenuNotification());
  }
}
