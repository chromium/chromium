// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ADD_USERNAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ADD_USERNAME_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/add_username_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class EditableCombobox;
}  // namespace views

// A view offering the user ability to add username to credentials that have it
// missing. Contains a username and password field. In addition, it contains a
// "Save" button and a "Nope" button.
class PasswordAddUsernameView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordAddUsernameView, PasswordBubbleViewBase)

 public:
  PasswordAddUsernameView(content::WebContents* web_contents,
                          views::View* anchor_view,
                          DisplayReason reason);

 private:
  ~PasswordAddUsernameView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // PasswordBubbleViewBase:
  views::View* GetInitiallyFocusedView() override;
  ui::ImageModel GetWindowIcon() override;

  // View:
  void AddedToWidget() override;

  void UpdateUsernameInModel();
  std::unique_ptr<views::View> CreateFooterView();
  void OnUsernameChanged();

  AddUsernameBubbleController controller_;
  raw_ptr<views::EditableCombobox> username_dropdown_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ADD_USERNAME_VIEW_H_
