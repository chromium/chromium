// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"

class WebUIBrowserWindow;

// The delegate for the WebContents managing the UI in WebUI-based browser.
class WebUIBrowserWebContentsDelegate : public content::WebContentsDelegate,
                                        public content::WebContentsObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void DraggableRegionsChanged(
        const std::vector<blink::mojom::DraggableRegionPtr>& regions) = 0;
  };

  explicit WebUIBrowserWebContentsDelegate(WebUIBrowserWindow* window);
  ~WebUIBrowserWebContentsDelegate() override;

  void SetUIWebContents(content::WebContents* ui_web_contents);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  WebUIBrowserWindow* window() { return window_; }

 private:
  // WebContentsDelegate implementation.
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void SetFocusToLocationBar() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  raw_ptr<WebUIBrowserWindow> window_;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WEB_CONTENTS_DELEGATE_H_
