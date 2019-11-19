// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/supports_user_data.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/web_content_controller.h"
#include "url/gurl.h"

namespace chromecast {
class CastWebContents;
}  // namespace chromecast

namespace content {
class BrowserContext;
class WebContents;
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace chromecast {

class WebviewNavigationThrottle;

// This owns a WebContents and CastWebContents and processes proto commands
// to allow the web contents to be controlled and embedded.
class WebviewController : public CastWebContents::Delegate,
                          public CastWebContents::Observer,
                          public WebContentController {
 public:
  WebviewController(content::BrowserContext* browser_context, Client* client);
  ~WebviewController() override;

  // Returns a navigation throttle for the current navigation request, if one is
  // necessary.
  static std::unique_ptr<content::NavigationThrottle>
  MaybeGetNavigationThrottle(content::NavigationHandle* handle);

  // Cause the controller to be destroyed after giving the webpage a chance to
  // run unload events. This unsets the client so no more messages will be
  // sent.
  void Destroy() override;

  void ProcessRequest(const webview::WebviewRequest& request) override;

  // Close the page. This will cause a stopped response to eventually be sent.
  void ClosePage();

  void SendNavigationEvent(WebviewNavigationThrottle* throttle,
                           const GURL& url,
                           bool is_in_main_frame);

 protected:
  content::WebContents* GetWebContents() override;

 private:
  webview::AsyncPageEvent_State current_state();

  // CastWebContents::Delegate
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;
  void OnPageStopped(CastWebContents* cast_web_contents,
                     int error_code) override;

  // CastWebContents::Observer
  void ResourceLoadFailed(CastWebContents* cast_web_contents) override;

  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<CastWebContents> cast_web_contents_;
  bool stopped_ = false;

  // The navigation throttle for the current navigation event, if any.
  // Is set only:
  //    When has_navigation_delegate is true, and
  //    A NavigationEvent call is currently in process.
  // Cleared immediately after the NavigationDecision has been processed.
  WebviewNavigationThrottle* current_navigation_throttle_ =
      nullptr;  // Not owned.

  base::WeakPtrFactory<WebviewController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebviewController);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
