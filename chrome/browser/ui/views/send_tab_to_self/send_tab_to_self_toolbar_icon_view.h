// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller_delegate.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

class Browser;
class BrowserView;

namespace send_tab_to_self {

// STTS icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is a received
// STTS notification.
class SendTabToSelfToolbarIconView
    : public views::ImageView,
      public SendTabToSelfToolbarIconControllerDelegate {
  METADATA_HEADER(SendTabToSelfToolbarIconView, views::ImageView)

 public:
  explicit SendTabToSelfToolbarIconView(BrowserView* browser_view);
  SendTabToSelfToolbarIconView(const SendTabToSelfToolbarIconView&) = delete;
  SendTabToSelfToolbarIconView& operator=(const SendTabToSelfToolbarIconView&) =
      delete;
  ~SendTabToSelfToolbarIconView() override;

  // SendTabToSelfToolbarIconControllerDelegate implementation.
  void Show(const SendTabToSelfEntry& entry) override;
  bool IsActive() override;

  void DismissEntry(std::string& guid);

 private:
  const raw_ptr<const Browser> browser_;

  raw_ptr<const BrowserView> browser_view_;

  raw_ptr<const SendTabToSelfEntry> entry_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_VIEW_H_
