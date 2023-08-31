// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_

namespace content {
class WebContents;
}

namespace autofill {
class AutofillBubbleBase;
class LocalCardMigrationBubbleController;
class OfferNotificationBubbleController;
class SaveUpdateAddressProfileBubbleController;
class EditAddressProfileDialogController;
class SaveCardBubbleController;
class IbanBubbleController;
class VirtualCardManualFallbackBubbleController;
class VirtualCardEnrollBubbleController;
class MandatoryReauthBubbleController;
enum class IbanBubbleType;
enum class MandatoryReauthBubbleType;

// TODO(crbug.com/1337392): consider removing this class and give the logic back
// to each bubble's controller. This class serves also the avatar button /
// personal data manager observer for saving feedback. If we end up not doing it
// the same way, this class may be unnecessary.
// Responsible for receiving calls from controllers and showing autofill
// bubbles.
class AutofillBubbleHandler {
 public:
  AutofillBubbleHandler() = default;

  AutofillBubbleHandler(const AutofillBubbleHandler&) = delete;
  AutofillBubbleHandler& operator=(const AutofillBubbleHandler&) = delete;

  virtual ~AutofillBubbleHandler() = default;

  virtual AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowIbanBubble(content::WebContents* web_contents,
                                             IbanBubbleController* controller,
                                             bool is_user_gesture,
                                             IbanBubbleType bubble_type) = 0;

  virtual AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* web_contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowEditAddressProfileDialog(
      content::WebContents* web_contents,
      EditAddressProfileDialogController* controller) = 0;

  virtual AutofillBubbleBase* ShowVirtualCardManualFallbackBubble(
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowMandatoryReauthBubble(
      content::WebContents* web_contents,
      MandatoryReauthBubbleController* controller,
      bool is_user_gesture,
      MandatoryReauthBubbleType bubble_type) = 0;

  // TODO(crbug.com/964127): Wait for the integration with sign in after local
  // save to be landed to see if we need to merge password saved and credit card
  // saved functions.
  virtual void OnPasswordSaved() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
