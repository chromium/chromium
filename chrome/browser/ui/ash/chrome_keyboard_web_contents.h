// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_WEB_CONTENTS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_WEB_CONTENTS_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

class ChromeKeyboardBoundsObserver;

// WebContents manager for the virtual keyboard. This observes the web
// contents, manages the content::HostZoomMap, and informs the virtual
// keyboard controller when the contents have loaded. It also provides a
// WebContentsDelegate implementation.
class ChromeKeyboardWebContents : public content::WebContentsObserver {
 public:
  using LoadCallback = base::OnceCallback<void()>;

  // Immediately starts loading |url| in a WebContents. |callback| is called
  // when the WebContents finishes loading.
  ChromeKeyboardWebContents(content::BrowserContext* context,
                            const GURL& url,
                            LoadCallback callback);
  ~ChromeKeyboardWebContents() override;

  // Updates the keyboard URL if |url| does not match the existing url.
  void SetKeyboardUrl(const GURL& url);

  // Provide access to the native view (aura::Window) and frame
  // (RenderWidgetHostView) through WebContents. TODO(stevenjb): Remove this
  // once host window ownership is moved to ash.
  content::WebContents* web_contents() { return web_contents_.get(); }

  ChromeKeyboardBoundsObserver* window_bounds_observer() {
    return window_bounds_observer_.get();
  }

 private:
  // content::WebContentsObserver overrides
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Loads the web contents for the given |url|.
  void LoadContents(const GURL& url);

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ChromeKeyboardBoundsObserver> window_bounds_observer_;
  LoadCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardWebContents);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_WEB_CONTENTS_H_
