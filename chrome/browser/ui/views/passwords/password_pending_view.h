// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class Label;
class ToggleImageButton;
}  // namespace views

class DesktopIOSPromotionBubbleView;
class PasswordSignInPromoView;

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|). Contains a username and password field, along with a
// "Save"/"Update" button and a "Never"/"Nope" button.
class PasswordPendingView : public PasswordBubbleViewBase,
                            public views::ButtonListener,
                            public views::TextfieldController {
 public:
  PasswordPendingView(content::WebContents* web_contents,
                      views::View* anchor_view,
                      const gfx::Point& anchor_point,
                      DisplayReason reason);

#if defined(UNIT_TEST)
  const View* username_field() const { return username_field_; }
#endif

 private:
  ~PasswordPendingView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // PasswordBubbleViewBase:
  views::View* CreateFootnoteView() override;
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

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
  DesktopIOSPromotionBubbleView* desktop_ios_promo_;

  views::View* username_field_;
  views::ToggleImageButton* password_view_button_;
  views::View* initially_focused_view_;

  // The view for the password value. Only one of |password_dropdown_| and
  // |password_label_| should be available.
  views::Combobox* password_dropdown_;
  views::Label* password_label_;

  bool are_passwords_revealed_;

  DISALLOW_COPY_AND_ASSIGN(PasswordPendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_PENDING_VIEW_H_
