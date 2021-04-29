// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_

#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/token.h"
#include "chrome/browser/ui/passwords/bubble_controllers/save_update_with_account_store_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/view.h"

namespace views {
class AnimatingLayoutManager;
class Combobox;
class EditableCombobox;
class ToggleImageButton;
}  // namespace views

class FeaturePromoControllerViews;

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|) either in the profile and/or account stores. Contains
// a username and password field, and in case of a saving a destination picker.
// In addition, it contains a "Save"/"Update" button and a "Never"/"Nope"
// button.
class PasswordSaveUpdateWithAccountStoreView
    : public PasswordBubbleViewBase,
      public views::WidgetObserver,
      public views::AnimatingLayoutManager::Observer {
 public:
  PasswordSaveUpdateWithAccountStoreView(
      content::WebContents* web_contents,
      views::View* anchor_view,
      DisplayReason reason,
      FeaturePromoControllerViews* promo_controller);

  views::Combobox* DestinationDropdownForTesting() {
    return destination_dropdown_;
  }

  views::View* GetUsernameTextfieldForTest() const;

 private:
  // Type of the currently shown IPH.
  enum class IPHType {
    kNone,     // No IPH is shown.
    kRegular,  // The regular IPH introducing the user to destination picker.
    kFailedReauth,  // The IPH shown after reauth failure informing the user
                    // about the switch to local mode.
  };
  class AutoResizingLayout;
  ~PasswordSaveUpdateWithAccountStoreView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // PasswordBubbleViewBase:
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  gfx::ImageSkia GetWindowIcon() override;

  // View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // views::AnimatingLayoutManager::Observer:
  void OnLayoutIsAnimatingChanged(views::AnimatingLayoutManager* source,
                                  bool is_animating) override;

  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void UpdateBubbleUIElements();
  void UpdateHeaderImage();

  void DestinationChanged();

  // Whether we should shown an IPH upon account reauth failure that informs the
  // user that the destination has been automatically switched to device.
  bool ShouldShowFailedReauthIPH();

  // Tries to show an IPH bubble of |type|. For kFailedReauth,
  // ShouldShowFailedReauthIPH() should be checked first.
  void MaybeShowIPH(IPHType type);

  void CloseIPHBubbleIfOpen();

  // Announces to the screen readers a change in the bubble between Save and
  // Update states.
  void AnnounceSaveUpdateChange();

  // Used for both the username and password editable comboboxes.
  void OnContentChanged();

  SaveUpdateWithAccountStoreBubbleController controller_;

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  views::Combobox* destination_dropdown_ = nullptr;

  views::EditableCombobox* username_dropdown_ = nullptr;
  views::ToggleImageButton* password_view_button_ = nullptr;

  // The view for the password value.
  views::EditableCombobox* password_dropdown_ = nullptr;
  bool are_passwords_revealed_;

  // Used to display IPH. May be null in tests.
  FeaturePromoControllerViews* const promo_controller_;

  // When showing kReauthFailure IPH, |promo_controller_| gives back an
  // ID. This is used to close the bubble later.
  base::Optional<base::Token> failed_reauth_promo_id_;

  // Hidden view that will contain status text for immediate output by
  // screen readers when the bubble changes state between Save and Update.
  views::View* accessibility_alert_ = nullptr;

  // Used to add |username_dropdown_| as an observer to the
  // AnimatingLayoutManager. This is needed such that the |username_dropdown_|
  // keeps the dropdown menu closed while the layout is animating.
  std::unique_ptr<
      base::ScopedObservation<views::AnimatingLayoutManager,
                              views::AnimatingLayoutManager::Observer>>
      animating_layout_for_username_dropdown_observation_;

  // Used to observe the bubble animation when transitions between Save/Update
  // states. If appropriate, IPH bubble is is shown st end of the animation.
  base::ScopedObservation<views::AnimatingLayoutManager,
                          views::AnimatingLayoutManager::Observer>
      animating_layout_for_iph_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_
