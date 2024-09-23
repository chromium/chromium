// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_HELPER_H_
#define COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/content/captive_portal_tab_reloader.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace net {
class SSLInfo;
}

class CaptivePortalBrowserTest;

namespace captive_portal {

class CaptivePortalLoginDetector;
class CaptivePortalTabReloader;

// Along with the classes it owns, responsible for detecting page loads broken
// by a captive portal, triggering captive portal checks on navigation events
// that may indicate a captive portal is present, or has been removed / logged
// in to, and taking any correcting actions. Lives on the UI thread.
//
// It acts as a WebContentsObserver for its CaptivePortalLoginDetector and
// CaptivePortalTabReloader. It filters out non-main-frame navigations. It is
// also needed by CaptivePortalTabReloaders to inform the tab's
// CaptivePortalLoginDetector when the tab is at a captive portal's login page.
//
// The TabHelper assumes that a WebContents can only have one main frame
// navigation at a time. This assumption can be violated in rare cases, for
// example, a same-site navigation interrupted by a cross-process navigation
// started from the omnibox, may commit before it can be cancelled.  In these
// cases, this class may pass incorrect messages to the TabReloader, which
// will, at worst, result in not opening up a login tab until a second load
// fails or not automatically reloading a tab after logging in.
// TODO(clamy): See if this class can be made to handle these edge-cases
// following the refactor of navigation signaling to WebContentsObservers.
//
// For the design doc, see:
// https://docs.google.com/document/d/1k-gP2sswzYNvryu9NcgN7q5XrsMlUdlUdoW9WRaEmfM/edit
class CaptivePortalTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CaptivePortalTabHelper> {
 public:
  CaptivePortalTabHelper(const CaptivePortalTabHelper&) = delete;
  CaptivePortalTabHelper& operator=(const CaptivePortalTabHelper&) = delete;

  ~CaptivePortalTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;

  // Called when a certificate interstitial error page is about to be shown.
  void OnSSLCertError(const net::SSLInfo& ssl_info);

  // A "Login Tab" is a tab that was originally at a captive portal login
  // page.  This is set to false when a captive portal is no longer detected.
  bool IsLoginTab() const;

  // Called to indicate a tab is at, or is navigating to, the captive portal
  // login page.
  void SetIsLoginTab();

  bool is_captive_portal_window() const {
    return window_type_ == CaptivePortalWindowType::kPopup;
  }
  bool is_captive_portal_tab() const {
    return window_type_ == CaptivePortalWindowType::kTab;
  }
  void set_window_type(CaptivePortalWindowType window_type) {
    window_type_ = window_type;
  }

 private:
  friend class ::CaptivePortalBrowserTest;
  friend class CaptivePortalTabHelperTest;

  friend class content::WebContentsUserData<CaptivePortalTabHelper>;
  CaptivePortalTabHelper(content::WebContents* web_contents,
                         CaptivePortalService* captive_portal_service,
                         const CaptivePortalTabReloader::OpenLoginTabCallback&
                             open_login_tab_callback);
  void Observe(const CaptivePortalService::Results& results);

  // Called by Observe in response to the corresponding event.
  void OnCaptivePortalResults(CaptivePortalResult previous_result,
                              CaptivePortalResult result);

  // |this| takes ownership of |tab_reloader|.
  void SetTabReloaderForTest(CaptivePortalTabReloader* tab_reloader);

  CaptivePortalTabReloader* GetTabReloaderForTest();

  // The current main frame navigation happening for the WebContents, or
  // nullptr if there is none. If there are two main frame navigations
  // happening at once, it's the one that started most recently.
  raw_ptr<content::NavigationHandle> navigation_handle_ = nullptr;

  // Neither of these will ever be NULL.
  std::unique_ptr<CaptivePortalTabReloader> tab_reloader_;
  std::unique_ptr<CaptivePortalLoginDetector> login_detector_;

  // Whether this tab is part of a window that was constructed for captive
  // portal resolution.
  CaptivePortalWindowType window_type_ = CaptivePortalWindowType::kNone;

  base::CallbackListSubscription subscription_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_HELPER_H_
