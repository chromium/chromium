// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_window.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"

namespace ash {

class NetworkPortalSigninWindow::WindowObserver
    : public content::WebContentsObserver {
 public:
  WindowObserver(content::WebContents* web_contents,
                 NetworkPortalSigninWindow* controller)
      : content::WebContentsObserver(web_contents), controller_(controller) {}
  ~WindowObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    RequestPortalDetection();
  }

  void WebContentsDestroyed() override { RequestPortalDetection(); }

 private:
  void RequestPortalDetection() {
    NET_LOG(EVENT) << "Request portal detection";
    controller_->portal_detection_requested_for_testing_++;
    NetworkHandler::Get()->network_state_handler()->RequestPortalDetection();
  }

 private:
  raw_ptr<NetworkPortalSigninWindow> controller_;
};

// static
NetworkPortalSigninWindow* NetworkPortalSigninWindow::Get() {
  static base::NoDestructor<NetworkPortalSigninWindow> instance;
  return instance.get();
}

NetworkPortalSigninWindow::NetworkPortalSigninWindow() = default;

NetworkPortalSigninWindow::~NetworkPortalSigninWindow() = default;

void NetworkPortalSigninWindow::Show(const GURL& url) {
  if (BrowserDelegate* browser = GetBrowser()) {
    NET_LOG(EVENT) << "Show existing portal signin window";
    browser->LoadUrl(url, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                     {.show_window = true});
    return;
  }

  NET_LOG(EVENT) << "Show new portal signin window";
  const auto& session =
      CHECK_DEREF(session_manager::SessionManager::Get()->GetActiveSession());
  content::WebContents* contents =
      BrowserController::GetInstance()->OpenCaptivePortalSignin(
          session.account_id(), url,
          BrowserController::CaptivePortalWindowMode::kPopup);
  if (!contents) {
    NET_LOG(ERROR) << "Failed to navigate to captive portal url: " << url;
    window_session_id_ = SessionID::InvalidValue();
    return;
  }
  window_session_id_ =
      CHECK_DEREF(BrowserController::GetInstance()->GetBrowserForTab(contents))
          .GetSessionID();
  window_observer_ = std::make_unique<WindowObserver>(contents, this);
}

BrowserDelegate* NetworkPortalSigninWindow::GetBrowser() {
  BrowserDelegate* result = nullptr;
  BrowserController::GetInstance()->ForEachBrowser(
      BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](BrowserDelegate& browser) {
        if (browser.GetSessionID() == window_session_id_) {
          result = &browser;
          return BrowserController::kBreakIteration;
        }
        return BrowserController::kContinueIteration;
      });
  return result;
}

BrowserDelegate* NetworkPortalSigninWindow::GetBrowserForTesting() {
  CHECK_IS_TEST();
  return GetBrowser();
}

content::WebContents* NetworkPortalSigninWindow::GetWebContentsForTesting() {
  CHECK_IS_TEST();
  if (!window_observer_.get()) {
    return nullptr;
  }
  return window_observer_->web_contents();
}

}  // namespace ash
