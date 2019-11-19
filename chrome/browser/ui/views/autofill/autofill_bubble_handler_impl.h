// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
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
class LocalCardMigrationBubble;
class LocalCardMigrationBubbleController;
class SaveCardBubbleView;
class SaveCardBubbleController;

class AutofillBubbleHandlerImpl : public AutofillBubbleHandler,
                                  public PersonalDataManagerObserver,
                                  public AvatarToolbarButton::Observer {
 public:
  AutofillBubbleHandlerImpl(Browser* browser,
                            ToolbarButtonProvider* toolbar_button_provider);
  ~AutofillBubbleHandlerImpl() override;

  // AutofillBubbleHandler:
  SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  SaveCardBubbleView* ShowSaveCardSignInPromoBubble(
      content::WebContents* contents,
      SaveCardBubbleController* controller) override;
  LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  void OnPasswordSaved() override;
  void HideSignInPromo() override;

  // PersonalDataManagerObserver:
  void OnCreditCardSaved(bool should_show_sign_in_promo_if_applicable) override;

  // AvatarToolbarButton::Observer:
  void OnAvatarHighlightAnimationFinished() override;

 private:
  // Executes highlight animation on toolbar's avatar icon.
  void ShowAvatarHighlightAnimation();

  Browser* browser_ = nullptr;

  ToolbarButtonProvider* toolbar_button_provider_ = nullptr;

  // Whether a save local card sign in promo bubble could pop up from the avatar
  // button after the highlight animation finishes.
  bool should_show_sign_in_promo_if_applicable_ = false;

  ScopedObserver<PersonalDataManager, PersonalDataManagerObserver>
      personal_data_manager_observer_{this};
  ScopedObserver<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_toolbar_button_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillBubbleHandlerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
