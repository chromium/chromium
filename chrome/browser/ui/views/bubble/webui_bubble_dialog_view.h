// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// A Views bubble host for a WebUIBubbleView.
class WebUIBubbleDialogView : public views::BubbleDialogDelegateView,
                              public WebUIBubbleView::Host {
 public:
  static base::WeakPtr<WebUIBubbleDialogView> CreateWebUIBubbleDialog(
      std::unique_ptr<WebUIBubbleDialogView> bubble_view);

  WebUIBubbleDialogView(views::View* anchor_view,
                        std::unique_ptr<WebUIBubbleView> web_view);
  WebUIBubbleDialogView(const WebUIBubbleDialogView&) = delete;
  WebUIBubbleDialogView& operator=(const WebUIBubbleDialogView&) = delete;
  ~WebUIBubbleDialogView() override;

  std::unique_ptr<WebUIBubbleView> RemoveWebView();
  WebUIBubbleView* web_view() { return web_view_; }

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // WebUIBubbleView::Host:
  void ShowUI() override;
  void CloseUI() override;
  void OnWebViewSizeChanged() override;

 private:
  WebUIBubbleView* web_view_ = nullptr;
  base::WeakPtrFactory<WebUIBubbleDialogView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
