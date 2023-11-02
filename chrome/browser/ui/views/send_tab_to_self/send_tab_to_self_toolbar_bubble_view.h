// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;
struct NavigateParams;

namespace send_tab_to_self {

class SendTabToSelfEntry;
class SendTabToSelfToolbarIconView;

class SendTabToSelfToolbarBubbleView : public views::BubbleDialogDelegateView {
 public:
  ~SendTabToSelfToolbarBubbleView() override;

  // Creates and shows the bubble.
  static SendTabToSelfToolbarBubbleView* CreateBubble(
      Profile* profile,
      SendTabToSelfToolbarIconView* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

 private:
  friend class SendTabToSelfToolbarBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfToolbarBubbleViewTest,
                           ButtonNavigatesToPage);

  SendTabToSelfToolbarBubbleView(
      Profile* profile,
      SendTabToSelfToolbarIconView* parent,
      const SendTabToSelfEntry& entry,
      base::OnceCallback<void(NavigateParams*)> navigate_callback);

  void OpenInNewTab();

  void Timeout();

  void Hide();

  // The button that owns |this|.
  raw_ptr<SendTabToSelfToolbarIconView> toolbar_button_;

  base::OnceCallback<void(NavigateParams*)> navigate_callback_;

  bool opened_ = false;

  raw_ptr<Profile> profile_;

  std::string title_;
  GURL url_;
  std::string device_name_;
  std::string guid_;

  base::WeakPtrFactory<SendTabToSelfToolbarBubbleView> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
