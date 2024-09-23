// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;

namespace ui {
class LayerTreeOwner;
}

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

  // Sets the status bubble, which should be repositioned every time
  // this view changes visible bounds.
  void SetStatusBubble(StatusBubbleViews* status_bubble);
  StatusBubbleViews* GetStatusBubble() const;

  // Toggles whether the background is visible.
  void SetBackgroundVisible(bool background_visible);

  const gfx::RoundedCornersF& background_radii() const {
    return background_radii_;
  }
  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  // WebView overrides:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
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
  raw_ptr<StatusBubbleViews> status_bubble_;

  bool background_visible_ = true;

  gfx::RoundedCornersF background_radii_;

  std::unique_ptr<ui::LayerTreeOwner> cloned_layer_tree_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
