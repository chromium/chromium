// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BrowserWindowInterface;

namespace send_tab_to_self {

class SendTabToSelfToolbarBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(SendTabToSelfToolbarBubbleView,
                  views::BubbleDialogDelegateView)

 public:
  SendTabToSelfToolbarBubbleView(BrowserWindowInterface& browser,
                                 views::BubbleAnchor anchor,
                                 const SendTabToSelfEntry& entry);

  ~SendTabToSelfToolbarBubbleView() override;

  // Creates and shows the bubble.
  static SendTabToSelfToolbarBubbleView* CreateBubble(
      BrowserWindowInterface& browser,
      views::BubbleAnchor anchor,
      const SendTabToSelfEntry& entry);

  // Overwrites the existing entry in the bubble with `new_entry`.
  void ReplaceEntry(const SendTabToSelfEntry& new_entry);
  void Hide();

  void OpenInNewTab();

  std::string GetGuidForTesting() { return entry_.GetGUID(); }

 private:
  friend class SendTabToSelfToolbarBubbleViewTest;
  friend class SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest;
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfToolbarBubbleViewTest,
                           ButtonNavigatesToPage);
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfToolbarBubbleViewTest,
                           ButtonNavigatesWithScrollPosition);
  FRIEND_TEST_ALL_PREFIXES(
      SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest,
      ButtonNavigatesWithoutScrollPositionIfFeatureDisabled);

  void Timeout();

  bool opened_ = false;

  const raw_ref<BrowserWindowInterface> browser_;

  raw_ptr<views::Label> title_label_;
  raw_ptr<views::Label> url_label_;
  raw_ptr<views::Label> device_label_;

  SendTabToSelfEntry entry_;

  base::WeakPtrFactory<SendTabToSelfToolbarBubbleView> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_VIEW_H_
