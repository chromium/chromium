// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;
struct NavigateParams;

namespace send_tab_to_self {

class SendTabToSelfEntry;
class SendTabToSelfToolbarButtonView;

class SendTabToSelfToolbarBubbleView : public views::BubbleDialogDelegateView {
 public:
  ~SendTabToSelfToolbarBubbleView() override;

  // Creates and shows the bubble.
  static SendTabToSelfToolbarBubbleView* CreateBubble(
      Profile* profile,
      SendTabToSelfToolbarButtonView* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

 private:
  friend class SendTabToSelfToolbarBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfToolbarBubbleViewTest,
                           ButtonNavigatesToPage);

  SendTabToSelfToolbarBubbleView(
      Profile* profile,
      SendTabToSelfToolbarButtonView* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

  void OpenInNewTab();

  void Hide();

  // The button that owns |this|.
  SendTabToSelfToolbarButtonView* toolbar_button_;

  base::OnceCallback<void(NavigateParams*)> navigate_callback_;

  bool opened_ = false;

  Profile* profile_;

  std::string title_;
  GURL url_;
  std::string device_name_;
  std::string guid_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
