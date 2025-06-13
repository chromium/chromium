// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/magic_boost_user_consent_view.h"

#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_layout_provider.h"

namespace quick_answers {

TEST(MagicBoostUserConsentViewTest, ButtonTextLabel) {
  views::test::TestLayoutProvider test_layout_provider;
  chromeos::ReadWriteCardsUiController read_write_cards_ui_controller;
  MagicBoostUserConsentView magic_boost_user_consent_view(
      /*chip_label=*/u"testing label", read_write_cards_ui_controller);

  EXPECT_EQ(u"testing label",
            magic_boost_user_consent_view.chip_label_for_testing());
}

}  // namespace quick_answers
