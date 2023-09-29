// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

class Browser;
class ToolbarButtonProvider;

namespace content {
class WebContents;
}

namespace autofill {
class AutofillBubbleBase;
class LocalCardMigrationBubbleController;
class SaveCardBubbleController;
class IbanBubbleController;
enum class IbanBubbleType;

class AutofillBubbleHandlerImpl : public AutofillBubbleHandler,
                                  public AvatarToolbarButton::Observer {
 public:
  AutofillBubbleHandlerImpl(Browser* browser,
                            ToolbarButtonProvider* toolbar_button_provider);

  AutofillBubbleHandlerImpl(const AutofillBubbleHandlerImpl&) = delete;
  AutofillBubbleHandlerImpl& operator=(const AutofillBubbleHandlerImpl&) =
      delete;

  ~AutofillBubbleHandlerImpl() override;

  // AutofillBubbleHandler:
  AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowIbanBubble(content::WebContents* web_contents,
                                     IbanBubbleController* controller,
                                     bool is_user_gesture,
                                     IbanBubbleType bubble_type) override;

  AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardManualFallbackBubble(
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowMandatoryReauthBubble(
      content::WebContents* web_contents,
      MandatoryReauthBubbleController* controller,
      bool is_user_gesture,
      MandatoryReauthBubbleType bubble_type) override;

  // AvatarToolbarButton::Observer:
  void OnAvatarHighlightAnimationFinished() override;

 private:
  // Executes highlight animation on toolbar's avatar icon.
  void ShowAvatarHighlightAnimation();

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<ToolbarButtonProvider> toolbar_button_provider_ = nullptr;

  // Whether a save local card sign in promo bubble could pop up from the avatar
  // button after the highlight animation finishes.
  bool should_show_sign_in_promo_if_applicable_ = false;

  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_toolbar_button_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
