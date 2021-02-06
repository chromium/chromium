// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;

namespace ui {
class LayerTreeOwner;
}

// ContentsWebView is used to present the WebContents of the active tab.
class ContentsWebView
    : public views::WebView,
      public WebContentsCloseHandlerDelegate {
 public:
  explicit ContentsWebView(content::BrowserContext* browser_context);
  ~ContentsWebView() override;

  // Sets the status bubble, which should be repositioned every time
  // this view changes visible bounds.
  void SetStatusBubble(StatusBubbleViews* status_bubble);

  // WebView overrides:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;
  void RenderViewReady() override;
  void OnLetterboxingChanged() override;

  // ui::View overrides:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  // WebContentsCloseHandlerDelegate overrides:
  void CloneWebContentsLayer() override;
  void DestroyClonedLayer() override;

 private:
  void UpdateBackgroundColor();
  StatusBubbleViews* status_bubble_;

  std::unique_ptr<ui::LayerTreeOwner> cloned_layer_tree_;

  DISALLOW_COPY_AND_ASSIGN(ContentsWebView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
