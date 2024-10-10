// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_layout_provider.h"

namespace quick_answers {

TEST(UserConsentViewTest, A11yInfo) {
  views::test::TestLayoutProvider test_layout_provider;
  chromeos::ReadWriteCardsUiController read_write_cards_ui_controller;
  UserConsentView user_consent_view(/*use_refreshed_design=*/false,
                                    read_write_cards_ui_controller);
  user_consent_view.SetIntentText(u"test");
  user_consent_view.SetIntentType(IntentType::kDictionary);

  EXPECT_EQ(u"Get the definition for \"test\" and more",
            user_consent_view.GetAccessibleName());
  EXPECT_EQ(
      u"Get definitions, translations, or unit conversions when you "
      u"right-click or touch & hold text Use Left or Right arrow keys to "
      u"manage this feature.",
      user_consent_view.GetAccessibleDescription());
}

TEST(UserConsentViewTest, A11yInfoRefreshed) {
  views::test::TestLayoutProvider test_layout_provider;
  chromeos::ReadWriteCardsUiController read_write_cards_ui_controller;
  UserConsentView user_consent_view(/*use_refreshed_design=*/true,
                                    read_write_cards_ui_controller);
  user_consent_view.SetIntentText(u"test");
  user_consent_view.SetIntentType(IntentType::kDictionary);

  EXPECT_EQ(u"Define \"test\"", user_consent_view.GetAccessibleName());
  EXPECT_EQ(
      u"Right-click or press and hold to get definitions, translations, or "
      u"unit conversions Use Left or Right arrow keys to manage this feature.",
      user_consent_view.GetAccessibleDescription());
}

}  // namespace quick_answers
