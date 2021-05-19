// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_VIEW_H_

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;
class BrowserView;

namespace send_tab_to_self {

// STTS icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is a received
// STTS notification.
class SendTabToSelfToolbarButtonView
    : public ToolbarButton,
      public SendTabToSelfToolbarButtonControllerDelegate {
 public:
  explicit SendTabToSelfToolbarButtonView(BrowserView* browser_view);
  SendTabToSelfToolbarButtonView(const SendTabToSelfToolbarButtonView&) =
      delete;
  SendTabToSelfToolbarButtonView& operator=(
      const SendTabToSelfToolbarButtonView&) = delete;
  ~SendTabToSelfToolbarButtonView() override;

  // SendTabToSelfToolbarButtonControllerDelegate implementation.
  void Show() override;
  void Hide() override;

 private:
  void ButtonPressed();

  const Browser* const browser_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_VIEW_H_
