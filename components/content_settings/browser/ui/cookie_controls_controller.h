// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace content_settings {

class CookieSettings;
class CookieControlsObserver;
class OldCookieControlsObserver;

// Handles the tab specific state for cookie controls.
class CookieControlsController
    : content_settings::CookieSettings::Observer,
      public base::SupportsWeakPtr<CookieControlsController> {
 public:
  CookieControlsController(
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<content_settings::CookieSettings> original_cookie_settings,
      HostContentSettingsMap* settings_map);
  CookieControlsController(const CookieControlsController& other) = delete;
  CookieControlsController& operator=(const CookieControlsController& other) =
      delete;
  ~CookieControlsController() override;

  // Called when the web_contents has changed.
  void Update(content::WebContents* web_contents);

  // Called when the UI is closing.
  void OnUiClosing();

  // Called when the user clicks on the button to enable/disable cookie
  // blocking.
  void OnCookieBlockingEnabledForSite(bool block_third_party_cookies);

  // Called when the entry point for cookie controls was animated.
  void OnEntryPointAnimated();

  // Returns whether first-party cookies are blocked.
  bool FirstPartyCookiesBlocked();

  // Returns whether, due to calls to OnCookingEnabledForSite(), the cookie
  // blocking setting for the current site is different than what it was when
  // the page was loaded.
  bool HasCookieBlockingChangedForSite();

  // Returns the current breakage confidence level.
  CookieControlsBreakageConfidenceLevel GetBreakageConfidenceLevel();

  // Returns the current cookie controls status.
  CookieControlsStatus GetCookieControlsStatus();

  void AddObserver(OldCookieControlsObserver* obs);
  void RemoveObserver(OldCookieControlsObserver* obs);

  void AddObserver(CookieControlsObserver* obs);
  void RemoveObserver(CookieControlsObserver* obs);

 private:
  struct Status {
    CookieControlsStatus status;
    CookieControlsEnforcement enforcement;
    base::Time expiration;
  };

  // The observed WebContents changes during the lifetime of the
  // CookieControlsController. SiteDataObserver can't change the observed
  // object, so we need an inner class that can be recreated when necessary.
  // TODO(dullweber): Make it possible to change the observed class and maybe
  // convert SiteDataObserver to a pure virtual interface.
  class TabObserver
      : public content_settings::PageSpecificContentSettings::SiteDataObserver,
        public content::WebContentsObserver {
   public:
    TabObserver(CookieControlsController* cookie_controls,
                content::WebContents* web_contents);

    TabObserver(const TabObserver&) = delete;
    TabObserver& operator=(const TabObserver&) = delete;
    ~TabObserver() override;

    // PageSpecificContentSettings::SiteDataObserver:
    void OnSiteDataAccessed(const AccessDetails& access_details) override;
    void OnStatefulBounceDetected() override;

    // content::WebContentsObserver:
    void PrimaryPageChanged(content::Page& page) override;
    void DidStopLoading() override;

   private:
    raw_ptr<CookieControlsController> cookie_controls_;
    base::RepeatingTimer timer_;

    // The last URL observed in `PrimaryPageChanged()`.
    GURL last_visited_url_;

    // The number of detected page reloads for |last_visited_url_| in the last
    // 30 seconds.
    int reload_count_ = 0;

    // Cache of cookie access details that have been already reported for the
    // current page load.
    std::set<AccessDetails> cookie_accessed_set_;

    void ResetReloadCounter();
  };

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;
  void OnCookieSettingChanged() override;

  // Determine the CookieControlsStatus based on |web_contents|.
  Status GetStatus(content::WebContents* web_contents);

  // Determine the confidence of site being broken and user needing to use
  // cookie controls. It affects the prominence of UI entry points. It takes
  // into account blocked third-party cookie access, exceptions
  // lifecycle, site engagement index and recent user activity (like frequent
  // page reloads).
  CookieControlsBreakageConfidenceLevel GetConfidenceLevel(
      CookieControlsStatus status,
      int allowed_sites,
      int blocked_sites,
      int bounce_count);

  // Updates the blocked cookie count of |icon_|.
  void PresentBlockedCookieCounter();

  void OnPageReloadDetected(int recent_reloads_count);

  void OnPageFinishedLoading();

  // Returns the number of allowed cookies.
  int GetAllowedCookieCount() const;

  // Returns the number of blocked cookies.
  int GetBlockedCookieCount() const;

  // Returns the number of stateful bounces leading to this page.
  int GetStatefulBounceCount() const;

  // Returns the number of allowed sites.
  int GetAllowedSitesCount() const;

  // Returns the number of blocked sites.
  int GetBlockedSitesCount() const;

  // Returns the number of allowed third-party sites with cookies.
  int GetAllowedThirdPartyCookiesSitesCount() const;

  // Returns the number of blocked third-party sites with cookies.
  int GetBlockedThirdPartyCookiesSitesCount() const;

  double GetSiteEngagementScore();

  // Record metrics when third-party cookies are allowed.
  void RecordActivationMetrics();

  void ResetInitialCookieControlsStatus();

  content::WebContents* GetWebContents() const;

  std::unique_ptr<TabObserver> tab_observer_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Cookie_settings for the original profile associated with
  // |cookie_settings_|, if there is one. For example, in Chrome, this
  // corresponds to the regular profile when |cookie_settings_| is incognito.
  // This may be null.
  scoped_refptr<content_settings::CookieSettings> original_cookie_settings_;
  raw_ptr<HostContentSettingsMap> settings_map_;

  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_observation_{this};

  bool should_reload_ = false;

  // The number of page reloads in last 30 seconds.
  int recent_reloads_count_ = 0;

  bool has_exception_expired_since_last_visit_ = false;

  bool waiting_for_page_load_finish_ = false;

  // Record the initial control status when the page was navigated to, to allow
  // querying of whether the effective cookie control status has changed.
  CookieControlsStatus initial_page_cookie_controls_status_ =
      CookieControlsStatus::kUninitialized;

  base::ObserverList<OldCookieControlsObserver> old_observers_;
  base::ObserverList<CookieControlsObserver> observers_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_
