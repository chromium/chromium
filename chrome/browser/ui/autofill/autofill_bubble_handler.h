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
class AutofillBubbleBase;
class LocalCardMigrationBubbleController;
class OfferNotificationBubbleController;
class SaveAddressProfileBubbleController;
class EditAddressProfileDialogController;
class SaveCardBubbleController;
class SaveUPIBubble;
class SaveUPIBubbleController;

// Responsible for receiving calls from controllers and showing autofill
// bubbles.
class AutofillBubbleHandler {
 public:
  AutofillBubbleHandler() = default;
  virtual ~AutofillBubbleHandler() = default;

  virtual AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* web_contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual SaveUPIBubble* ShowSaveUPIBubble(
      content::WebContents* contents,
      SaveUPIBubbleController* controller) = 0;

  virtual AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      SaveAddressProfileBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowEditAddressProfileDialog(
      content::WebContents* web_contents,
      EditAddressProfileDialogController* controller) = 0;

  // TODO(crbug.com/964127): Wait for the integration with sign in after local
  // save to be landed to see if we need to merge password saved and credit card
  // saved functions.
  virtual void OnPasswordSaved() = 0;

  // TODO(crbug.com/964127): Move password bubble here.

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillBubbleHandler);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
