// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_RELOADER_H_
#define COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_RELOADER_H_

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "net/dns/public/resolve_error_info.h"

namespace content {
class WebContents;
}

namespace net {
class SSLInfo;
}

class CaptivePortalBrowserTest;
class CaptivePortalForPrerenderingTest;

namespace captive_portal {

// Keeps track of whether a tab has encountered a navigation error caused by a
// captive portal.  Also triggers captive portal checks when a page load may
// have been broken or be taking longer due to a captive portal.  All methods
// may only be called on the UI thread.
//
// Only supports SSL main frames which end at error pages as a result of
// captive portals, since these make for a particularly bad user experience.
// Non-SSL requests are intercepted by captive portals, which take users to the
// login page.  SSL requests, however, may be silently blackholed, or result
// in a variety of error pages, and will continue to do so if a user tries to
// reload them.
class CaptivePortalTabReloader {
 public:
  enum State {
    STATE_NONE,
    // The slow load timer is running.  Only started on SSL provisional loads.
    // If the timer triggers before the page has been committed, a captive
    // portal test will be requested.
    STATE_TIMER_RUNNING,
    // The tab may have been broken by a captive portal.  A tab switches to
    // this state either on a main frame SSL error that may be caused by a
    // captive portal, or when an SSL request takes too long to commit.  The
    // tab will remain in this state until the current load succeeds, a new
    // provisional load starts, it gets a captive portal result, or the load
    // fails with error that indicates the page was not broken by a captive
    // portal.
    STATE_MAYBE_BROKEN_BY_PORTAL,
    // The TabHelper switches to this state from STATE_MAYBE_BROKEN_BY_PORTAL in
    // response to a RESULT_BEHIND_CAPTIVE_PORTAL.  The tab will remain in this
    // state until a new provisional load starts, the original load successfully
    // commits, the current load is aborted, or the tab reloads the page in
    // response to receiving a captive portal result other than
    // RESULT_BEHIND_CAPTIVE_PORTAL.
    STATE_BROKEN_BY_PORTAL,
    // The page may need to be reloaded.  The tab will be reloaded if the page
    // fails the next load with a timeout, or immediately upon switching to this
    // state, if the page already timed out.  If anything else happens
    // when in this state (Another error, successful navigation, or the original
    // navigation was aborted), the TabHelper transitions to STATE_NONE without
    // reloading.
    STATE_NEEDS_RELOAD,
  };

  // Function to open a login tab, if there isn't one already.
  using OpenLoginTabCallback = base::RepeatingCallback<void()>;

  // |captive_portal_service| and |web_contents| will only be dereferenced in
  // ReloadTab, MaybeOpenCaptivePortalLoginTab, and CheckForCaptivePortal, so
  // they can both be NULL in the unit tests as long as those functions are not
  // called.
  CaptivePortalTabReloader(CaptivePortalService* captive_portal_service,
                           content::WebContents* web_contents,
                           const OpenLoginTabCallback& open_login_tab_callback);

  CaptivePortalTabReloader(const CaptivePortalTabReloader&) = delete;
  CaptivePortalTabReloader& operator=(const CaptivePortalTabReloader&) = delete;

  virtual ~CaptivePortalTabReloader();

  // The following functions are all invoked by the CaptivePortalTabHelper:

  // Called when a non-error main frame load starts.  Resets current state,
  // unless this is a login tab.  Each load will eventually result in a call to
  // OnLoadCommitted or OnAbort.  The former will be called both on successful
  // loads and for error pages.
  virtual void OnLoadStart(bool is_ssl);

  // Called when the main frame is committed. |net_error| will be net::OK in
  // the case of a successful load. |resolve_error_info| contains information
  // about any hostname resolution error. For an error page, the entire 3-step
  // process of getting the error, starting a new provisional load for the error
  // page, and committing the error page is treated as a single commit.
  virtual void OnLoadCommitted(int net_error,
                               net::ResolveErrorInfo resolve_error_info);

  // This is called when the current provisional main frame load is canceled.
  // Sets state to STATE_NONE, unless this is a login tab.
  virtual void OnAbort();

  // Called whenever a provisional load to the main frame is redirected.
  virtual void OnRedirect(bool is_ssl);

  // Called whenever a captive portal test completes.
  virtual void OnCaptivePortalResults(CaptivePortalResult previous_result,
                                      CaptivePortalResult result);

  // Called on certificate errors, which often indicate a captive portal.
  void OnSSLCertError(const net::SSLInfo& ssl_info);

 protected:
  // The following functions are used only when testing:

  State state() const { return state_; }

  content::WebContents* web_contents() { return web_contents_; }

  void set_slow_ssl_load_time(base::TimeDelta slow_ssl_load_time) {
    slow_ssl_load_time_ = slow_ssl_load_time;
  }

  // Started whenever an SSL tab starts loading, when the state is switched to
  // STATE_TIMER_RUNNING.  Stopped on any state change, including when a page
  // commits or there's an error.  If the timer triggers, the state switches to
  // STATE_MAYBE_BROKEN_BY_PORTAL and |this| kicks off a captive portal check.
  base::OneShotTimer slow_ssl_load_timer_;

 private:
  friend class ::CaptivePortalBrowserTest;
  friend class ::CaptivePortalForPrerenderingTest;

  // Sets |state_| and takes any action associated with the new state.  Also
  // stops the timer, if needed. If |new_state| is STATE_MAYBE_BROKEN_BY_PORTAL,
  // |probe_trigger| should be specified.
  void SetState(State new_state);

  // Called by a timer when an SSL main frame provisional load is taking a
  // while to commit.
  void OnSlowSSLConnect();

  // Called when a main frame loads with a secure DNS network error.
  void OnSecureDnsNetworkError();

  // Reloads the tab if there's no provisional load going on and the current
  // state is STATE_NEEDS_RELOAD.  Not safe to call synchronously when called
  // by from a WebContentsObserver function, since the WebContents is currently
  // performing some action.
  void ReloadTabIfNeeded();

  // Reloads the tab.
  virtual void ReloadTab();

  // Opens a login tab in the topmost browser window for the
  // |captive_portal_service_|, if the captive_portal_service has a tabbed
  // browser window and the window doesn't already have a login tab.  Otherwise,
  // does nothing.
  virtual void MaybeOpenCaptivePortalLoginTab();

  // Has |captive_portal_service_| (if present) start a captive portal check.
  virtual void CheckForCaptivePortal();

  raw_ptr<CaptivePortalService> captive_portal_service_;
  raw_ptr<content::WebContents> web_contents_;

  State state_;

  // Tracks if there's a load going on that can't safely be interrupted.  This
  // is true between the time when a provisional load fails and when an error
  // page's provisional load starts, so does not perfectly align with the
  // notion of a provisional load used by the WebContents.
  bool provisional_main_frame_load_;

  // True if there was an SSL URL the in the redirect chain for the current
  // provisional main frame load.
  bool ssl_url_in_redirect_chain_;

  // Time to wait after a provisional HTTPS load before triggering a captive
  // portal check.
  base::TimeDelta slow_ssl_load_time_;

  const OpenLoginTabCallback open_login_tab_callback_;

  base::WeakPtrFactory<CaptivePortalTabReloader> weak_factory_{this};
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_TAB_RELOADER_H_
