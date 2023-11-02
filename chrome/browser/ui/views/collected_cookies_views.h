// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/window/dialog_delegate.h"

class CookieInfoView;
class CookiesTreeModel;
class CookiesTreeViewDrawingProvider;
class InfobarView;
class PageSpecificSiteDataDialogController;

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
class CollectedCookiesViews : public views::DialogDelegateView,
                              public views::TabbedPaneListener,
                              public views::TreeViewController {
 public:
  METADATA_HEADER(CollectedCookiesViews);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabbedPaneElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBlockedCookiesTreeElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowedCookiesTreeElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBlockButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRemoveButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kClearOnExitButtonId);

  CollectedCookiesViews(const CollectedCookiesViews&) = delete;
  CollectedCookiesViews& operator=(const CollectedCookiesViews&) = delete;
  ~CollectedCookiesViews() override;

  void set_status_changed_for_testing() { status_changed_ = true; }

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // views::TreeViewController:
  void OnTreeViewSelectionChanged(views::TreeView* tree_view) override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

 private:
  friend class PageSpecificSiteDataDialogController;

  explicit CollectedCookiesViews(content::WebContents* web_contents);

  void OnDialogClosed();

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

  void DeleteSelectedCookieNode();

  // The web contents.
  base::WeakPtr<content::WebContents> web_contents_;

  // Assorted views.
  raw_ptr<views::Label> allowed_label_ = nullptr;
  raw_ptr<views::Label> blocked_label_ = nullptr;

  raw_ptr<views::TreeView> allowed_cookies_tree_ = nullptr;
  raw_ptr<views::TreeView> blocked_cookies_tree_ = nullptr;

  raw_ptr<views::LabelButton> block_allowed_button_ = nullptr;
  raw_ptr<views::LabelButton> delete_allowed_button_ = nullptr;
  raw_ptr<views::LabelButton> allow_blocked_button_ = nullptr;
  raw_ptr<views::LabelButton> for_session_blocked_button_ = nullptr;

  std::unique_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  std::unique_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  raw_ptr<CookiesTreeViewDrawingProvider> allowed_cookies_drawing_provider_ =
      nullptr;
  raw_ptr<CookiesTreeViewDrawingProvider> blocked_cookies_drawing_provider_ =
      nullptr;

  raw_ptr<CookieInfoView> cookie_info_view_ = nullptr;

  raw_ptr<InfobarView> infobar_ = nullptr;

  // Weak pointers to the allowed and blocked panes so that they can be
  // shown/hidden as needed.
  raw_ptr<views::View> allowed_buttons_pane_ = nullptr;
  raw_ptr<views::View> blocked_buttons_pane_ = nullptr;

  bool status_changed_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
