// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/password_form.h"

class PageSwitcherView;
namespace views {
class Textarea;
class Textfield;
}

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
  void AddedToWidget() override;
  bool Cancel() override;
  bool Accept() override;

  std::unique_ptr<views::View> CreatePasswordListTitleView() const;
  std::unique_ptr<views::View> CreatePasswordListView();
  std::unique_ptr<views::View> CreatePasswordDetailsView();
  std::unique_ptr<views::View> CreatePasswordDetailsTitleView();
  std::unique_ptr<views::View> CreateFooterView();

  // Changes the contents of the page to either display the details of
  // `currently_selected_password_` or the list of passwords when
  // `currently_selected_password_` isn't set.
  void RecreateLayout();

  void SwitchToEditUsernameMode();
  void SwitchToEditNoteMode();

  void SwitchToDisplayMode();

  // Called when the favicon is loaded. If |favicon| isn't empty, it sets
  // |favicon_| and invokes RecreateLayout().
  void OnFaviconReady(const gfx::Image& favicon);

  // Returns the image model representing site favicon. If favicon is empty or
  // not loaded yet, it returns the image model of the globe icon.
  ui::ImageModel GetFaviconImageModel() const;

  // Holds the favicon of the page when it is asynchronously loaded.
  gfx::Image favicon_;

  // If not set, the bubble displays the list of all credentials stored for the
  // current domain. When set, the bubble displays the password details of the
  // currently selected password.
  absl::optional<password_manager::PasswordForm> currently_selected_password_;

  raw_ptr<views::View> display_username_row_ = nullptr;
  raw_ptr<views::View> edit_username_row_ = nullptr;
  views::Textfield* username_textfield_ = nullptr;

  raw_ptr<views::View> display_note_row_ = nullptr;
  raw_ptr<views::View> edit_note_row_ = nullptr;
  views::Textarea* note_textarea_ = nullptr;

  ItemsBubbleController controller_;
  raw_ptr<PageSwitcherView> page_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
