// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/supports_user_data.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/cast_web_contents_observer.h"
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
class WebviewTest;

// This owns a WebContents and CastWebContents and processes proto commands
// to allow the web contents to be controlled and embedded.
class WebviewController : public CastWebContentsObserver,
                          public WebContentController {
 public:
  WebviewController(content::BrowserContext* browser_context,
                    Client* client,
                    bool enabled_for_dev);
  WebviewController(std::unique_ptr<content::BrowserContext> browser_context,
                    Client* client,
                    bool enabled_for_dev);

  WebviewController(const WebviewController&) = delete;
  WebviewController& operator=(const WebviewController&) = delete;

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

  // Dispatch a navigation request event with the information supplied in the
  // navigation handle.
  void SendNavigationEvent(WebviewNavigationThrottle* throttle,
                           content::NavigationHandle* navigation_handle);
  void OnNavigationThrottleDestroyed(WebviewNavigationThrottle* throttle);

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, Focus);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, KeyInput);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, SendFocusEventWhenVKShouldBeShown);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, SetInsets);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, UserDataOverrideOnFirstRequest);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, UserDataOverride);
  FRIEND_TEST_ALL_PREFIXES(WebviewTest, VerifyNavigationDelegation);

  content::WebContents* GetWebContents() override;

 private:
  void HandleLoadUrl(const webview::NavigateRequest& request);
  void HandleUpdateSettings(const webview::UpdateSettingsRequest& request);
  void HandleSetAutoMediaPlaybackPolicy(
      const webview::SetAutoMediaPlaybackPolicyRequest& request);

  webview::AsyncPageEvent_State current_state();

  // CastWebContentsObserver implementation:
  void PageStateChanged(PageState page_state) override;
  void PageStopped(PageState page_state, int error_code) override;
  void ResourceLoadFailed() override;

  // content::WebContentsObserver
  void DidFirstVisuallyNonEmptyPaint() override;

  // BrowserContext instances must outlive their WebContents, so destroy this
  // last.
  std::unique_ptr<content::BrowserContext> owned_context_;

  const bool enabled_for_dev_;
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<CastWebContents> cast_web_contents_;
  PageState page_state_ = PageState::IDLE;
  bool stopped_ = false;

  // The navigation throttle for the current navigation event, if any.
  // Is set only:
  //    When has_navigation_delegate is true, and
  //    A NavigationEvent call is currently in process.
  // Cleared immediately after the NavigationDecision has been processed.
  WebviewNavigationThrottle* current_navigation_throttle_ =
      nullptr;  // Not owned.

  base::WeakPtrFactory<WebviewController> weak_ptr_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
