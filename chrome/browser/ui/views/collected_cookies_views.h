// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/window/dialog_delegate.h"

class CookieInfoView;
class CookiesTreeModel;
class CookiesTreeViewDrawingProvider;
class InfobarView;

namespace content {
class WebContents;
}

namespace views {
class Label;
class LabelButton;
class TreeView;
}

// This is the Views implementation of the collected cookies dialog.
//
// CollectedCookiesViews is a dialog that displays the allowed and blocked
// cookies of the current tab contents. To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the WebContents's
// content settings tab helper.
class CollectedCookiesViews
    : public views::DialogDelegateView,
      public views::ButtonListener,
      public views::TabbedPaneListener,
      public views::TreeViewController,
      public content::WebContentsUserData<CollectedCookiesViews> {
 public:
  ~CollectedCookiesViews() override;

  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  static void CreateAndShowForWebContents(content::WebContents* web_contents);

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  void DeleteDelegate() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // views::TreeViewController:
  void OnTreeViewSelectionChanged(views::TreeView* tree_view) override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

 private:
  friend class CollectedCookiesViewsTest;
  friend class content::WebContentsUserData<CollectedCookiesViews>;

  explicit CollectedCookiesViews(content::WebContents* web_contents);

  void Init();

  std::unique_ptr<views::View> CreateAllowedPane();

  std::unique_ptr<views::View> CreateBlockedPane();

  // Creates and returns the "buttons pane", which is the view in the
  // bottom-leading edge of this dialog containing the action buttons for the
  // currently selected host or cookie.
  std::unique_ptr<views::View> CreateButtonsPane();

  // Creates and returns a containing ScrollView around the given tree view.
  std::unique_ptr<views::View> CreateScrollView(
      std::unique_ptr<views::TreeView> pane);

  void EnableControls();

  void ShowCookieInfo();

  void AddContentException(views::TreeView* tree_view, ContentSetting setting);

  // The web contents.
  content::WebContents* web_contents_;

  // Assorted views.
  views::Label* allowed_label_ = nullptr;
  views::Label* blocked_label_ = nullptr;

  views::TreeView* allowed_cookies_tree_ = nullptr;
  views::TreeView* blocked_cookies_tree_ = nullptr;

  views::LabelButton* block_allowed_button_ = nullptr;
  views::LabelButton* delete_allowed_button_ = nullptr;
  views::LabelButton* allow_blocked_button_ = nullptr;
  views::LabelButton* for_session_blocked_button_ = nullptr;

  std::unique_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  std::unique_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  CookiesTreeViewDrawingProvider* allowed_cookies_drawing_provider_ = nullptr;
  CookiesTreeViewDrawingProvider* blocked_cookies_drawing_provider_ = nullptr;

  CookieInfoView* cookie_info_view_ = nullptr;

  InfobarView* infobar_ = nullptr;

  // Weak pointers to the allowed and blocked panes so that they can be
  // shown/hidden as needed.
  views::View* allowed_buttons_pane_ = nullptr;
  views::View* blocked_buttons_pane_ = nullptr;

  bool status_changed_ = false;

  // This bit is set to true when the widget is shutting down or when |this|'s
  // destructor has been called. Either the Widget or the WebContents may be the
  // first to shut down, and this prevents double-destruction of this or
  // double-closing of the widget.
  bool destroying_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
