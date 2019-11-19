// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"

namespace autofill {

TestAutofillBubbleHandler::TestAutofillBubbleHandler() = default;

TestAutofillBubbleHandler::~TestAutofillBubbleHandler() = default;

SaveCardBubbleView* TestAutofillBubbleHandler::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  if (!save_card_bubble_view_)
    save_card_bubble_view_ = std::make_unique<TestSaveCardBubbleView>();
  return save_card_bubble_view_.get();
}

SaveCardBubbleView* TestAutofillBubbleHandler::ShowSaveCardSignInPromoBubble(
    content::WebContents* contents,
    autofill::SaveCardBubbleController* controller) {
  if (!save_card_bubble_view_)
    save_card_bubble_view_ = std::make_unique<TestSaveCardBubbleView>();
  return save_card_bubble_view_.get();
}

LocalCardMigrationBubble*
TestAutofillBubbleHandler::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  if (!local_card_migration_bubble_view_) {
    local_card_migration_bubble_view_ =
        std::make_unique<TestLocalCardMigrationBubbleView>();
  }
  return local_card_migration_bubble_view_.get();
}

void TestAutofillBubbleHandler::OnPasswordSaved() {}

void TestAutofillBubbleHandler::HideSignInPromo() {}

}  // namespace autofill
