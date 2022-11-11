// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

// A dialog for managing stored password and federated login information for a
// specific site. A user can see the details of the passwords, and edit the
// stored password note.
class ManagePasswordsView : public PasswordBubbleViewBase {
 public:
  ManagePasswordsView(content::WebContents* web_contents,
                      views::View* anchor_view);

  ManagePasswordsView(const ManagePasswordsView&) = delete;
  ManagePasswordsView& operator=(const ManagePasswordsView&) = delete;

  ~ManagePasswordsView() override;

 private:
  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  std::unique_ptr<views::View> CreateFooterView();

  ItemsBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
