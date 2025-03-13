// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "ui/views/view.h"

// Wrapper for a static method to format the TabSharingInfoBar status message.
// TODO(crbug.com/380903159): Implementat this View to represent the
// TabSharingInfoBar status message text with quick links to the shared and
// capturing tab.
class TabSharingStatusMessageView : public views::View {
  METADATA_HEADER(TabSharingStatusMessageView, views::View)

 public:
  static std::u16string GetMessageText(
      const std::u16string& shared_tab_name,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
