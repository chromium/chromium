// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/token.h"
#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/user_education/common/help_bubble.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/view.h"

namespace views {
class AnimatingLayoutManager;
class Combobox;
class EditableCombobox;
class EditablePasswordCombobox;
}  // namespace views

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|) either in the profile and/or account stores. Contains
// a username and password field, and in case of a saving a destination picker.
// In addition, it contains a "Save"/"Update" button and a "Never"/"Nope"
// button.
class PasswordSaveUpdateView : public PasswordBubbleViewBase,
                               public views::WidgetObserver,
                               public views::AnimatingLayoutManager::Observer {
 public:
  PasswordSaveUpdateView(content::WebContents* web_contents,
                         views::View* anchor_view,
                         DisplayReason reason);

  views::Combobox* DestinationDropdownForTesting() {
    return destination_dropdown_;
  }

  views::EditableCombobox* username_dropdown_for_testing() const {
    return username_dropdown_.get();
  }

 private:
  // Type of the IPH to show.
  enum class IPHType {
    kRegular,  // The regular IPH introducing the user to destination picker.
    kFailedReauth,  // The IPH shown after reauth failure informing the user
                    // about the switch to local mode.
  };
  class AutoResizingLayout;
  ~PasswordSaveUpdateView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // PasswordBubbleViewBase:
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  ui::ImageModel GetWindowIcon() override;

  // View:
  void AddedToWidget() override;

  // views::AnimatingLayoutManager::Observer:
  void OnLayoutIsAnimatingChanged(views::AnimatingLayoutManager* source,
                                  bool is_animating) override;
  void UpdateUsernameAndPasswordInModel();
  void UpdateBubbleUIElements();
  std::unique_ptr<views::View> CreateFooterView();

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

  // Should be called only after the bubble has been displayed.
  void UpdateFootnote();

  // Invoked when the user clicks on the eye image button in the password
  // dropdown. It invokes the controller to determine if the user should be able
  // to unmask the password.
  void TogglePasswordRevealed();

  SaveUpdateBubbleController controller_;

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  raw_ptr<views::Combobox> destination_dropdown_ = nullptr;

  // The views for the username and password dropdown elements.
  raw_ptr<views::EditableCombobox> username_dropdown_ = nullptr;
  raw_ptr<views::EditablePasswordCombobox> password_dropdown_ = nullptr;

  // When showing kReauthFailure IPH, the promo controller gives back an
  // ID. This is used to close the bubble later.
  std::unique_ptr<user_education::HelpBubble> failed_reauth_promo_bubble_;

  // Hidden view that will contain status text for immediate output by
  // screen readers when the bubble changes state between Save and Update.
  raw_ptr<views::View> accessibility_alert_ = nullptr;

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

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
