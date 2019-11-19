// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/google_password_manager_navigation_throttle.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using NavigationResult =
    GooglePasswordManagerNavigationThrottle::NavigationResult;
using content::NavigationThrottle;
using content::WebContents;
using content::WebContentsObserver;

constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(2);

// Helper method to make sure we don't dereference a destroyed WebContents
// below. This is effectively a copy of the test-only
// content::WebContentsDestroyedObserver.
class WebContentsDestroyedObserver : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~WebContentsDestroyedObserver() override = default;

  bool IsDestroyed() const { return destroyed_; }

 private:
  // WebContentsObserver:
  void WebContentsDestroyed() override { destroyed_ = true; }

  bool destroyed_ = false;
};

// This method implements an offline fallback by closing the tab corresponding
// to the initial navigation to https://passwords.google.com, and then
// navigating to chrome://settings/passwords instead.
// Note: This is done async so that the original navigation gets opportunity to
// properly tear down before the new navigation is initiated.
void PostShowPasswordManagerAndCloseTab(
    content::NavigationHandle* navigation_handle,
    const base::Location& from_here = base::Location::Current()) {
  base::PostTask(
      from_here, {content::BrowserThread::UI},
      base::BindOnce(
          [](std::unique_ptr<WebContentsDestroyedObserver> observer) {
            if (observer->IsDestroyed())
              return;

            WebContents* web_contents = observer->web_contents();
            Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
            chrome::ShowPasswordManager(browser);

            // Note: Since ShowPasswordManager() will result in either a new tab
            // or focus an existing instance of the password settings page, the
            // tab for the initial navigation to passwords.google.com is no
            // longer needed. Thus we close it.
            TabStripModel* tab_strip_model = browser->tab_strip_model();
            int index = tab_strip_model->GetIndexOfWebContents(web_contents);
            if (index == TabStripModel::kNoTab) {
              DLOG(ERROR) << "Could not find index of web contents.";
              return;
            }

            tab_strip_model->CloseWebContentsAt(
                index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
          },

          std::make_unique<WebContentsDestroyedObserver>(
              navigation_handle->GetWebContents())));
}

void RecordNavigationResult(NavigationResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.GooglePasswordManager.NavigationResult", result);
}

void RecordSuccessOrFailure(NavigationResult result,
                            base::TimeTicks navigation_start) {
  RecordNavigationResult(result);
  switch (result) {
    case NavigationResult::kSuccess:
      base::UmaHistogramTimes(
          "PasswordManager.GooglePasswordManager.TimeToSuccess",
          base::TimeTicks::Now() - navigation_start);
      return;
    case NavigationResult::kFailure:
      base::UmaHistogramTimes(
          "PasswordManager.GooglePasswordManager.TimeToFailure",
          base::TimeTicks::Now() - navigation_start);
      return;
    case NavigationResult::kTimeout:
      break;
  }

  NOTREACHED();
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle>
GooglePasswordManagerNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  WebContents* web_contents = handle->GetWebContents();
  // Don't create a throttle if the user is not navigating to the Google
  // Password Manager.
  if (handle->GetURL().GetOrigin() != GURL(chrome::kGooglePasswordManagerURL))
    return nullptr;

  // Also don't create a throttle if the user should not manage their passwords
  // in the Google Password Manager (e.g. the user is not syncing passwords).
  if (!ShouldManagePasswordsinGooglePasswordManager(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    return nullptr;
  }

  // Lastly, don't create a throttle if the navigation does not originate from
  // clicking a link (e.g. the user entered the URL in the omnibar).
  if (!ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                    ui::PAGE_TRANSITION_LINK)) {
    return nullptr;
  }

  return std::make_unique<GooglePasswordManagerNavigationThrottle>(handle);
}

GooglePasswordManagerNavigationThrottle::
    ~GooglePasswordManagerNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillStartRequest() {
  timer_.Start(FROM_HERE, kTimeout, this,
               &GooglePasswordManagerNavigationThrottle::OnTimeout);
  navigation_start_ = base::TimeTicks::Now();
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillFailRequest() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    RecordSuccessOrFailure(NavigationResult::kFailure, navigation_start_);
    PostShowPasswordManagerAndCloseTab(navigation_handle());
  }
  return NavigationThrottle::CANCEL;
}

NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillProcessResponse() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    RecordSuccessOrFailure(NavigationResult::kSuccess, navigation_start_);
    return NavigationThrottle::PROCEED;
  }

  return NavigationThrottle::CANCEL;
}

const char* GooglePasswordManagerNavigationThrottle::GetNameForLogging() {
  return "GooglePasswordManagerNavigationThrottle";
}

void GooglePasswordManagerNavigationThrottle::OnTimeout() {
  RecordNavigationResult(NavigationResult::kTimeout);
  PostShowPasswordManagerAndCloseTab(navigation_handle());
}
