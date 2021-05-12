// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"
#include "content/public/browser/web_ui_controller.h"

class Browser;
class TabStripUIEmbedder;
class TabStripUIHandler;

extern const char kWebUITabIdDataType[];
extern const char kWebUITabGroupIdDataType[];

// The WebUI version of the tab strip in the browser. It is currently only
// supported on ChromeOS in tablet mode.
class TabStripUI : public content::WebUIController {
 public:
  explicit TabStripUI(content::WebUI* web_ui);
  ~TabStripUI() override;

  // Initialize TabStripUI with its embedder and the Browser it's
  // running in. Must be called exactly once. The WebUI won't work until
  // this is called.
  void Initialize(Browser* browser, TabStripUIEmbedder* embedder);

  // The embedder should call this whenever the result of
  // Embedder::GetLayout() changes.
  void LayoutChanged();

  // The embedder should call this whenever the tab strip gains keyboard focus.
  void ReceivedKeyboardFocus();

 private:
  void HandleThumbnailUpdate(int extension_tab_id,
                             ThumbnailTracker::CompressedThumbnailData image);

  TabStripUIHandler* handler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TabStripUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
