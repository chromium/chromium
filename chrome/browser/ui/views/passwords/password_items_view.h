// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_

#include <memory>
#include <vector>

#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/views/view.h"

// A dialog for managing stored password and federated login information for a
// specific site. A user can remove managed credentials for the site via this
// dialog.
class PasswordItemsView : public PasswordBubbleViewBase {
 public:
  PasswordItemsView(content::WebContents* web_contents,
                    views::View* anchor_view);

  PasswordItemsView(const PasswordItemsView&) = delete;
  PasswordItemsView& operator=(const PasswordItemsView&) = delete;

  ~PasswordItemsView() override;

 private:
  class PasswordRow;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  void NotifyPasswordFormAction(
      const password_manager::PasswordForm& password_form,
      PasswordBubbleControllerBase::PasswordAction action);
  void RecreateLayout();
  std::unique_ptr<views::View> CreateFooterView();

  // Called when the favicon is loaded. If |favicon| isn't empty, it sets
  // |favicon_| and invokes RecreateLayout().
  void OnFaviconReady(const gfx::Image& favicon);

  std::vector<std::unique_ptr<PasswordRow>> password_rows_;

  // Holds the favicon of the page when it is asynchronously loaded.
  gfx::Image favicon_;

  ItemsBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_ITEMS_VIEW_H_
