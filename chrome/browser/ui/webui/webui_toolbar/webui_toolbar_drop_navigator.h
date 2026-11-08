// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DROP_NAVIGATOR_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DROP_NAVIGATOR_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;
class GURL;

namespace webui_toolbar {

// Navigates the current tab to a URL or text dropped in the WebUI toolbar,
// filtering by where the drag started. The drag state is read from the
// observed toolbar WebContents.
class WebUIToolbarDropNavigator : public content::WebContentsObserver {
 public:
  WebUIToolbarDropNavigator(BrowserWindowInterface& browser,
                            content::WebContents* toolbar_contents);
  WebUIToolbarDropNavigator(const WebUIToolbarDropNavigator&) = delete;
  WebUIToolbarDropNavigator& operator=(const WebUIToolbarDropNavigator&) =
      delete;
  ~WebUIToolbarDropNavigator() override;

  void NavigateToDroppedUrl(const GURL& url);
  void NavigateToDroppedText(const std::string& text);

 private:
  // Helper to retrieve and reset the drag origin state.
  bool GetDragOriginatedFromRendererAndReset();

  // Opens `url` in the current tab on behalf of a drop. Navigations from
  // renderer-originated drags get an opaque initiator origin.
  void OpenDroppedUrl(const GURL& url, bool drag_originated_from_renderer);

  const raw_ref<BrowserWindowInterface> browser_;
};

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DROP_NAVIGATOR_H_
