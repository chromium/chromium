// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app {

namespace {

// Waits for a pop-up web contents to open.
class PopupObserver : public content::WebContentsObserver {
 public:
  explicit PopupObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }
  content::WebContents* popup() { return popup_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (!popup_ && disposition == WindowOpenDisposition::NEW_POPUP) {
      popup_ = new_contents;
      run_loop_.Quit();
    }
  }

  raw_ptr<content::WebContents> popup_ = nullptr;
  base::RunLoop run_loop_;
};

}  // namespace

// Open a popup window with the given URL and return its WebContents.
base::expected<content::WebContents*, std::string> OpenPopupOfSize(
    content::WebContents* contents,
    const GURL& url,
    int width,
    int height) {
  PopupObserver observer(contents);
  if (!content::ExecJs(
          contents,
          content::JsReplace(
              "window.open($1, '', 'popup=true, width=$2, height=$3')", url,
              width, height))) {
    return base::unexpected("window.open failed");
  }
  observer.Wait();
  return observer.popup();
}

}  // namespace web_app
