// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
struct NavigateParams;

namespace send_tab_to_self {

class SendTabToSelfEntry;

class SendTabToSelfToolbarBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(SendTabToSelfToolbarBubbleView,
                  views::BubbleDialogDelegateView)

 public:
  SendTabToSelfToolbarBubbleView(
      const Browser* browser,
      View* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

  ~SendTabToSelfToolbarBubbleView() override;

  // Creates and shows the bubble.
  static SendTabToSelfToolbarBubbleView* CreateBubble(
      const Browser* browser,
      View* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

  // Overwrites the existing entry in the bubble with `new_entry`.
  void ReplaceEntry(const SendTabToSelfEntry& new_entry);
  void Hide();

  std::string GetGuidForTesting() { return guid_; }

 private:
  friend class SendTabToSelfToolbarBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfToolbarBubbleViewTest,
                           ButtonNavigatesToPage);


  void OpenInNewTab();

  void Timeout();

  void LogNotificationOpened();
  void LogNotificationDismissed();

  // The button that owns |this|.
  // TODO(b/361445261): Update this to PinnedActionToolbarButton after
  // ToolbarPinning is fully launched.
  raw_ptr<View> toolbar_button_;

  base::OnceCallback<void(NavigateParams*)> navigate_callback_;

  bool opened_ = false;

  raw_ptr<const Browser> browser_;

  raw_ptr<views::Label> title_label_;
  raw_ptr<views::Label> url_label_;
  raw_ptr<views::Label> device_label_;

  std::string title_;
  GURL url_;
  std::string device_name_;
  std::string guid_;

  base::WeakPtrFactory<SendTabToSelfToolbarBubbleView> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
