// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "chrome/common/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;
class WebContentsCloseHandler;

namespace ui {
class LayerTreeOwner;
}  // namespace ui

// ContentsWebView is used to present the WebContents of the active tab.
class ContentsWebView : public views::WebView,
                        public WebContentsCloseHandlerDelegate {
  METADATA_HEADER(ContentsWebView, views::WebView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContentsWebViewElementId);

  explicit ContentsWebView(content::BrowserContext* browser_context);
  ContentsWebView(const ContentsWebView&) = delete;
  ContentsWebView& operator=(const ContentsWebView&) = delete;
  ~ContentsWebView() override;

  StatusBubbleViews* GetStatusBubble() const;
  WebContentsCloseHandler* GetWebContentsCloseHandler() const;

  // Toggles whether the background is visible.
  void SetBackgroundVisible(bool background_visible);

  const gfx::RoundedCornersF& GetBackgroundRadii() const;
  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  // WebView overrides:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnThemeChanged() override;
  void RenderViewReady() override;
  void OnLetterboxingChanged() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // ui::View overrides:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  // WebContentsCloseHandlerDelegate overrides:
  void CloneWebContentsLayer() override;
  void DestroyClonedLayer() override;

 private:
  void UpdateBackgroundColor();
  std::unique_ptr<StatusBubbleViews> status_bubble_ = nullptr;
  std::unique_ptr<WebContentsCloseHandler> web_contents_close_handler_ =
      nullptr;

  bool background_visible_ = true;

  std::unique_ptr<ui::LayerTreeOwner> cloned_layer_tree_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
