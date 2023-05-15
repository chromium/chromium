// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/interaction/element_identifier.h"

class PageSwitcherView;
class ManagePasswordsListView;
class ManagePasswordsDetailsView;

// A dialog for managing stored password and federated login information for a
// specific site. A user can see the details of the passwords, and edit the
// stored password note.
class ManagePasswordsView : public PasswordBubbleViewBase {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopView);

  ManagePasswordsView(content::WebContents* web_contents,
                      views::View* anchor_view);

  ManagePasswordsView(const ManagePasswordsView&) = delete;
  ManagePasswordsView& operator=(const ManagePasswordsView&) = delete;

  ~ManagePasswordsView() override;

  // Changes the contents of the page to display the details of `password_form`.
  // Used for testing only to bypass mocking authentication flow.
  void DisplayDetailsOfPasswordForTesting(
      password_manager::PasswordForm password_form);

 private:
  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;
  void AddedToWidget() override;
  bool Cancel() override;
  bool Accept() override;

  std::unique_ptr<ManagePasswordsListView> CreatePasswordListView();
  std::unique_ptr<ManagePasswordsDetailsView> CreatePasswordDetailsView();
  std::unique_ptr<views::View> CreateFooterView();

  // Changes the contents of the page to either display the details of
  // `currently_selected_password_` or the list of passwords when
  // `currently_selected_password_` isn't set.
  void RecreateLayout();

  void SwitchToReadingMode();
  void SwitchToListView();

  // Resets `auth_timer_` to restart counting till auto-navigation to the list
  // view. It should be called upon user activity to make sure the back
  // navigation doesn't happen while the user is interacting with the bubble.
  void ExtendAuthValidity();

  // Called when the favicon is loaded. If |favicon| isn't empty, it sets
  // |favicon_| and invokes RecreateLayout().
  void OnFaviconReady(const gfx::Image& favicon);

  // Returns the image model representing site favicon. If favicon is empty or
  // not loaded yet, it returns the image model of the globe icon.
  ui::ImageModel GetFaviconImageModel() const;

  // Requests the controller to start an OS user reauth to display the details
  // of `password_form`. If reauth is successful, the view switches to the
  // details view and displays the details of `password_form`. Otherwise, it
  // remains on the list view.
  void AuthenticateUserAndDisplayDetailsOf(
      password_manager::PasswordForm password_form);

  // Holds the favicon of the page when it is asynchronously loaded.
  gfx::Image favicon_;

  raw_ptr<ManagePasswordsDetailsView> password_details_view_ = nullptr;

  ItemsBubbleController controller_;
  raw_ptr<PageSwitcherView> page_container_ = nullptr;

  // Used to keep track of the time once the user passed the auth challenge to
  // navigate to the details view. Once it runs out, SwitchToListView() will be
  // run.
  base::RetainingOneShotTimer auth_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_H_
