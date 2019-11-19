// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

namespace autofill {
class LocalCardMigrationBubble;
class LocalCardMigrationBubbleController;
class SaveCardBubbleView;
class SaveCardBubbleController;

// Responsible for receiving calls from controllers and showing autofill
// bubbles.
class AutofillBubbleHandler {
 public:
  AutofillBubbleHandler() = default;
  virtual ~AutofillBubbleHandler() = default;

  virtual SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) = 0;

  // Shows the sign in promo bubble from the avatar button.
  virtual SaveCardBubbleView* ShowSaveCardSignInPromoBubble(
      content::WebContents* contents,
      SaveCardBubbleController* controller) = 0;

  virtual LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) = 0;

  // TODO(crbug.com/964127): Wait for the integration with sign in after local
  // save to be landed to see if we need to merge password saved and credit card
  // saved functions.
  virtual void OnPasswordSaved() = 0;

  virtual void HideSignInPromo() = 0;

  // TODO(crbug.com/964127): Move password bubble here.
  // TODO(crbug.com/964127): Add ShowSyncPromoBubble().

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillBubbleHandler);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
