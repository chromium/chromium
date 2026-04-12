// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_ZOOM_BUBBLE_MANAGER_WEBUI_BROWSER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_ZOOM_BUBBLE_MANAGER_WEBUI_BROWSER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_manager.h"

class WebUIBrowserWindow;

// A ZoomBubbleManager implementation that connects the zoom bubble to
// WebUIBrowserWindow.
class ZoomBubbleManagerWebUIBrowser : public ZoomBubbleManager {
 public:
  explicit ZoomBubbleManagerWebUIBrowser(WebUIBrowserWindow* window);
  ~ZoomBubbleManagerWebUIBrowser() override;

  // ZoomBubbleManager:
  views::BubbleAnchor GetZoomBubbleAnchor() override;
  gfx::NativeView GetNativeView() override;
  void UpdateLegacyPageActionIcon() override;
  std::u16string GetZoomActionAccessibleName() override;

 private:
  raw_ptr<WebUIBrowserWindow> window_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_ZOOM_BUBBLE_MANAGER_WEBUI_BROWSER_H_
