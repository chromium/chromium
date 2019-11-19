// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class EditableCombobox;
class Label;
}  // namespace views

// Standalone functions for creating username and password views.
std::unique_ptr<views::Label> CreateUsernameLabel(
    const autofill::PasswordForm& form);
std::unique_ptr<views::Label> CreatePasswordLabel(
    const autofill::PasswordForm& form,
    int federation_message_id,
    bool is_password_visible);
std::unique_ptr<views::EditableCombobox> CreateUsernameEditableCombobox(
    const autofill::PasswordForm& form);

// A dialog for managing stored password and federated login information for a
// specific site. A user can remove managed credentials for the site via this
// dialog.
class PasswordItemsView : public PasswordBubbleViewBase,
                          public views::ButtonListener {
 public:
  PasswordItemsView(content::WebContents* web_contents,
                    views::View* anchor_view,
                    DisplayReason reason);
  ~PasswordItemsView() override;

 private:
  class PasswordRow;

  void NotifyPasswordFormAction(
      const autofill::PasswordForm& password_form,
      ManagePasswordsBubbleModel::PasswordAction action);
  void RecreateLayout();

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  std::vector<std::unique_ptr<PasswordRow>> password_rows_;

  DISALLOW_COPY_AND_ASSIGN(PasswordItemsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_
