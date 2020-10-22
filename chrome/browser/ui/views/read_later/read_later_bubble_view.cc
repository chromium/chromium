// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/read_later/read_later_bubble_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

// ReadLaterBubbleView ---------------------------------------------------------

// static
base::WeakPtr<ReadLaterBubbleView> ReadLaterBubbleView::Show(
    const Browser* browser,
    views::View* anchor_view) {
  auto bubble_view =
      std::make_unique<ReadLaterBubbleView>(browser, anchor_view);
  auto weak_ptr = bubble_view->weak_factory_.GetWeakPtr();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view->side_panel()) {
    bubble_view->LoadURL<ReadLaterUI>(GURL(chrome::kChromeUIReadLaterURL));
    browser_view->side_panel()->AddContent(std::move(bubble_view));
  } else {
    views::WebBubbleDialogView::CreateWebBubbleDialog<ReadLaterUI>(
        std::move(bubble_view), GURL(chrome::kChromeUIReadLaterURL));
  }
  return weak_ptr;
}

ReadLaterBubbleView::ReadLaterBubbleView(const Browser* browser,
                                         views::View* anchor_view)
    : WebBubbleDialogView(browser->profile(), anchor_view) {}

ReadLaterBubbleView::~ReadLaterBubbleView() = default;

void ReadLaterBubbleView::ReadingListModelLoaded(
    const ReadingListModel* model) {}
