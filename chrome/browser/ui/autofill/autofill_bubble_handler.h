// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace content {
class WebContents;
}

namespace autofill_ai {
class SaveOrUpdateAutofillAiDataController;
}

namespace autofill {
class AutofillProfile;
class AutofillBubbleBase;
class OfferNotificationBubbleController;
class SaveAddressBubbleController;
class UpdateAddressBubbleController;
class SaveCardBubbleController;
class IbanBubbleController;
class FilledCardInformationBubbleController;
class VirtualCardEnrollBubbleController;
class MandatoryReauthBubbleController;
enum class IbanBubbleType;
enum class MandatoryReauthBubbleType;

// TODO(crbug.com/40229274): consider removing this class and give the logic
// back to each bubble's controller. This class serves also the avatar button /
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

  virtual AutofillBubbleBase* ShowIbanBubble(content::WebContents* web_contents,
                                             IbanBubbleController* controller,
                                             bool is_user_gesture,
                                             IbanBubbleType bubble_type) = 0;

  virtual AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* web_contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowSaveAutofillAiDataBubble(
      content::WebContents* web_contents,
      autofill_ai::SaveOrUpdateAutofillAiDataController* controller) = 0;

  // Opens a save address bubble. The bubble's lifecycle is controlled by its
  // widget, and the controller must handle the widget closing to invalidate
  // the returned pointer, see `SaveAddressBubbleController::OnBubbleClosed()`.
  // The bubble view takes ownership of the `controller`.
  virtual AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<SaveAddressBubbleController> controller,
      bool is_user_gesture) = 0;

  // Opens a promo bubble after an address save or update, offering to move the
  // address to account store if the user signs in through the bubble. This move
  // will be performed by the `move_address_callback`.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  virtual AutofillBubbleBase* ShowAddressSignInPromo(
      content::WebContents* web_contents,
      const AutofillProfile& autofill_profile) = 0;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Opens an update address bubble. The bubble's lifecycle is controlled by its
  // widget, and the controller must handle the widget closing to invalidate
  // the returned pointer, see
  // `UpdateAddressBubbleController::OnBubbleClosed()`. The bubble view takes
  // ownership of the `controller`.
  virtual AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<UpdateAddressBubbleController> controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowFilledCardInformationBubble(
      content::WebContents* web_contents,
      FilledCardInformationBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) = 0;

  virtual AutofillBubbleBase* ShowVirtualCardEnrollConfirmationBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller) = 0;

  virtual AutofillBubbleBase* ShowMandatoryReauthBubble(
      content::WebContents* web_contents,
      MandatoryReauthBubbleController* controller,
      bool is_user_gesture,
      MandatoryReauthBubbleType bubble_type) = 0;

  virtual AutofillBubbleBase* ShowSaveCardConfirmationBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller) = 0;

  virtual AutofillBubbleBase* ShowSaveIbanConfirmationBubble(
      content::WebContents* web_contents,
      IbanBubbleController* controller) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_H_
