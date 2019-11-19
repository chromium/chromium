// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"

namespace autofill {

class TestLocalCardMigrationBubbleView final : public LocalCardMigrationBubble {
  void Hide() override {}
};

class TestSaveCardBubbleView final : public SaveCardBubbleView {
  void Hide() override {}
};

class TestAutofillBubbleHandler : public AutofillBubbleHandler {
 public:
  TestAutofillBubbleHandler();
  ~TestAutofillBubbleHandler() override;

  // AutofillBubbleHandler:
  SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  SaveCardBubbleView* ShowSaveCardSignInPromoBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller) override;
  LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  void OnPasswordSaved() override;
  void HideSignInPromo() override;

 private:
  std::unique_ptr<TestLocalCardMigrationBubbleView>
      local_card_migration_bubble_view_;
  std::unique_ptr<TestSaveCardBubbleView> save_card_bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillBubbleHandler);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_
