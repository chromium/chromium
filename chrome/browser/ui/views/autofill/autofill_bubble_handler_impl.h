// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

class Browser;
class ToolbarButtonProvider;

namespace content {
class WebContents;
}

namespace autofill {
class AutofillBubbleBase;
class LocalCardMigrationBubbleController;
class SaveCardBubbleController;
class SaveUPIBubble;

class AutofillBubbleHandlerImpl : public AutofillBubbleHandler,
                                  public PersonalDataManagerObserver,
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
  AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) override;
  SaveUPIBubble* ShowSaveUPIBubble(
      content::WebContents* web_contents,
      SaveUPIBubbleController* controller) override;
  AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowEditAddressProfileDialog(
      content::WebContents* web_contents,
      EditAddressProfileDialogController* controller) override;
  AutofillBubbleBase* ShowVirtualCardManualFallbackBubble(
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) override;

  void OnPasswordSaved() override;

  // PersonalDataManagerObserver:
  void OnCreditCardSaved(bool should_show_sign_in_promo_if_applicable) override;

  // AvatarToolbarButton::Observer:
  void OnAvatarHighlightAnimationFinished() override;

 private:
  // Executes highlight animation on toolbar's avatar icon.
  void ShowAvatarHighlightAnimation();

  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;

  raw_ptr<ToolbarButtonProvider, DanglingUntriaged> toolbar_button_provider_ =
      nullptr;

  // Whether a save local card sign in promo bubble could pop up from the avatar
  // button after the highlight animation finishes.
  bool should_show_sign_in_promo_if_applicable_ = false;

  base::ScopedObservation<PersonalDataManager, PersonalDataManagerObserver>
      personal_data_manager_observation_{this};
  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_toolbar_button_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
