// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/token.h"
#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view.h"

namespace views {
class EditableCombobox;
class EditablePasswordCombobox;
}  // namespace views

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|) either in the profile and/or account stores. Contains
// a username and password field. In addition, it contains a "Save"/"Update"
// button and a "Never"/"Nope" button.
class PasswordSaveUpdateView : public PasswordBubbleViewBase,
                               public views::WidgetObserver {
  METADATA_HEADER(PasswordSaveUpdateView, PasswordBubbleViewBase)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordBubble);

  PasswordSaveUpdateView(content::WebContents* web_contents,
                         views::View* anchor_view,
                         DisplayReason reason);
#ifdef UNIT_TEST
  views::EditableCombobox* username_dropdown_for_testing() const {
    return username_dropdown_.get();
  }

  views::EditablePasswordCombobox* password_dropdown_for_testing() const {
    return password_dropdown_.get();
  }
#endif  // #ifdef UNIT_TEST

 private:
  ~PasswordSaveUpdateView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // views::WidgetDelegate override.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // If the sign in promo should be shown, it will remove the current contents
  // of the bubble and replace them with the sign in promo. Returns true if the
  // bubble is not replaced with the promo, and therefore closed. Returns false
  // if it is replaced, which will cause the bubble to stay open.
  bool CloseOrReplaceWithPromo();

  // PasswordBubbleViewBase:
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  ui::ImageModel GetWindowIcon() override;

  // View:
  void AddedToWidget() override;

  void UpdateUsernameAndPasswordInModel();
  void UpdateBubbleUIElements();
  std::unique_ptr<views::View> CreateFooterView();

  // Announces to the screen readers a change in the bubble between Save and
  // Update states, or the Sign-in promo.
  void AnnounceBubbleChange();

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

  // True if the bubble is showing the sign in promo. False if not.
  bool is_signin_promo_bubble_ = false;

  // The views for the username and password dropdown elements.
  raw_ptr<views::EditableCombobox> username_dropdown_ = nullptr;
  raw_ptr<views::EditablePasswordCombobox> password_dropdown_ = nullptr;

  // Hidden view that will contain status text for immediate output by
  // screen readers when the bubble changes state between Save and Update.
  raw_ptr<views::View> accessibility_alert_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
