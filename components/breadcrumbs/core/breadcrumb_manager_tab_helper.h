// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_TAB_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/page_transition_types.h"

namespace breadcrumbs {

// Name of DidStartNavigation event (see
// WebStateObserver/WebContentsObserver::DidStartNavigation).
extern const char kBreadcrumbDidStartNavigation[];

// Name of DidStartNavigation event (see
// WebStateObserver/WebContentsObserver::DidFinishNavigation).
extern const char kBreadcrumbDidFinishNavigation[];

// Name of PageLoaded event (see WebStateObserver::PageLoaded and
// WebContentsObserver::DidFinishLoad).
extern const char kBreadcrumbPageLoaded[];

// Name of DidChangeVisibleSecurityState event
// (see WebStateObserver/WebContentsObserver::DidChangeVisibleSecurityState).
extern const char kBreadcrumbDidChangeVisibleSecurityState[];

// Name of OnInfoBarAdded event
// (see infobars::InfoBarManager::Observer::OnInfoBarAdded).
extern const char kBreadcrumbInfobarAdded[];

// Name of OnInfoBarRemoved event
// (see infobars::InfoBarManager::Observer::OnInfoBarRemoved).
extern const char kBreadcrumbInfobarRemoved[];

// Name of OnInfoBarReplaced event
// (see infobars::InfoBarManager::Observer::OnInfoBarReplaced).
extern const char kBreadcrumbInfobarReplaced[];

// Name of Scroll event, logged when web contents scroll view finishes
// scrolling.
extern const char kBreadcrumbScroll[];

// Name of Zoom event, logged when web contents scroll view finishes zooming.
extern const char kBreadcrumbZoom[];

// Constants below represent metadata for breadcrumb events.

// Appended to |kBreadcrumbDidChangeVisibleSecurityState| event if page has bad
// SSL cert.
extern const char kBreadcrumbAuthenticationBroken[];

// Appended to |kBreadcrumbDidFinishNavigation| event if
// navigation is a download.
extern const char kBreadcrumbDownload[];

// Appended to |kBreadcrumbInfobarRemoved| if infobar removal is not animated.
extern const char kBreadcrumbInfobarNotAnimated[];

// Appended to |kBreadcrumbDidChangeVisibleSecurityState| event if page has
// passive mixed content (f.e. an http served image on https served page).
extern const char kBreadcrumbMixedContent[];

// Appended to |kBreadcrumbPageLoaded| event if page load has
// failed.
extern const char kBreadcrumbPageLoadFailure[];

// Appended to |kBreadcrumbDidStartNavigation| and
// |kBreadcrumbPageLoaded| event if the navigation url was Chrome's New Tab
// Page.
extern const char kBreadcrumbNtpNavigation[];

// Appended to |kBreadcrumbDidStartNavigation| and
// |kBreadcrumbPageLoaded| events if the navigation url had google.com host.
// Users tend to search and then navigate back and forth between search results
// page and landing page. And these back-forward navigations can cause crashes.
extern const char kBreadcrumbGoogleNavigation[];

// Appended to |kBreadcrumbPageLoaded| event if content is PDF.
extern const char kBreadcrumbPdfLoad[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was a client side redirect (f.e. window.open without user gesture).
extern const char kBreadcrumbRendererInitiatedByScript[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was renderer-initiated navigation with user gesture (f.e. link tap or
// widow.open with user gesture).
extern const char kBreadcrumbRendererInitiatedByUser[];

// Handles logging of Breadcrumb events associated with a tab (WebContents on
// desktop, WebState on iOS).
class BreadcrumbManagerTabHelper : public infobars::InfoBarManager::Observer {
 public:
  ~BreadcrumbManagerTabHelper() override;
  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

  // Returns a unique identifier to be used in breadcrumb event logs to identify
  // events associated with the underlying tab. This value is unique across this
  // application run only, is NOT persisted, and will change across launches.
  int GetUniqueId() const { return unique_id_; }

 protected:
  explicit BreadcrumbManagerTabHelper(
      infobars::InfoBarManager* infobar_manager);

  // Logs the breadcrumb event for a started navigation with |navigation_id|.
  void LogDidStartNavigation(int64_t navigation_id,
                             GURL url,
                             bool is_ntp_url,
                             bool is_renderer_initiated,
                             bool has_user_gesture,
                             ui::PageTransition page_transition);

  // Logs the breadcrumb event for a finished navigation with |navigation_id|.
  void LogDidFinishNavigation(int64_t navigation_id,
                              bool is_download,
                              int error_code);

  // Logs the breadcrumb event for loading the page at |url|.
  void LogPageLoaded(bool is_ntp_url,
                     GURL url,
                     bool page_load_success,
                     const std::string& contents_mime_type);

  // Logs the breadcrumb event for changing the visible security state.
  void LogDidChangeVisibleSecurityState(
      bool displayed_mixed_content,
      bool security_style_authentication_broken);

  // Logs the breadcrumb event for a renderer process being terminated.
  void LogRenderProcessGone();

  // Logs the given |event| for the associated tab.
  void LogEvent(const std::string& event);

  // Returns true if event that was sequentially emitted |count| times should be
  // logged. Some events (e.g., infobars replacements or scrolling) are emitted
  // sequentially multiple times. Logging each event will pollute breadcrumbs,
  // so this throttling function decides if event should be logged.
  bool ShouldLogRepeatedEvent(int count);

 private:
  // Logs the given |event| for the associated tab by retrieving the breadcrumb
  // manager from WebState (iOS) or WebContents (desktop). This should not be
  // used directly to log events; use LogEvent() instead.
  virtual void PlatformLogEvent(const std::string& event) = 0;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  // A unique identifier for this tab helper, used in breadcrumb event logs to
  // identify events associated with the underlying tab.
  int unique_id_ = -1;

  raw_ptr<infobars::InfoBarManager> infobar_manager_ = nullptr;
  // A counter which is incremented for each |OnInfoBarReplaced| call. This
  // value is reset when any other infobars::InfoBarManager::Observer callback
  // is received.
  int sequentially_replaced_infobars_ = 0;

  // Manages this object as an observer of infobars.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{this};
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_TAB_HELPER_H_
