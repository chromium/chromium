// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/view.h"

namespace views {
class EditableCombobox;
class ToggleImageButton;
}  // namespace views

class PasswordSignInPromoView;

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|). Contains a username and password field, along with a
// "Save"/"Update" button and a "Never"/"Nope" button.
class PasswordPendingView : public PasswordBubbleViewBase,
                            public views::ButtonListener,
                            public views::EditableComboboxListener {
 public:
  PasswordPendingView(content::WebContents* web_contents,
                      views::View* anchor_view,
                      DisplayReason reason);

  views::View* GetUsernameTextfieldForTest() const;

 private:
  ~PasswordPendingView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::EditableComboboxListener:
  // Used for both the username and password editable comboboxes.
  void OnContentChanged(views::EditableCombobox* editable_combobox) override;

  // PasswordBubbleViewBase:
  std::unique_ptr<views::View> CreateFootnoteView() override;
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void ReplaceWithPromo();

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  // Different promo dialogs that helps the user get access to credentials
  // across devices. One of these are non-null when the promotion dialog is
  // active.
  PasswordSignInPromoView* sign_in_promo_;

  views::EditableCombobox* username_dropdown_;
  views::ToggleImageButton* password_view_button_;

  // The view for the password value.
  views::EditableCombobox* password_dropdown_;

  bool are_passwords_revealed_;

  DISALLOW_COPY_AND_ASSIGN(PasswordPendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_
