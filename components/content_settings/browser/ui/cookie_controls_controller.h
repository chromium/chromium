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
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_observer.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace content_settings {

class CookieSettings;
class CookieControlsObserver;

// Handles the tab specific state for cookie controls.
class CookieControlsController final
    : content_settings::CookieSettings::Observer {
 public:
  CookieControlsController(
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<content_settings::CookieSettings> original_cookie_settings,
      HostContentSettingsMap* settings_map,
      privacy_sandbox::TrackingProtectionSettings*
          tracking_protection_settings);
  CookieControlsController(const CookieControlsController& other) = delete;
  CookieControlsController& operator=(const CookieControlsController& other) =
      delete;
  ~CookieControlsController() override;

  // Called when the web_contents has changed.
  void Update(content::WebContents* web_contents);

  // Called when the fingerprinting protection filter has blocked a subresource.
  void OnSubresourceBlocked();

  // Called when the UI is closing.
  void OnUiClosing();

  // Called when the user clicks on the button to enable/disable cookie
  // blocking.
  void OnCookieBlockingEnabledForSite(bool block_third_party_cookies);

  // Called when the entry point for cookie controls was animated.
  void OnEntryPointAnimated();

  // Returns whether any ACT features should be shown.
  bool ShowActFeatures();

  // Returns whether the cookie blocking setting for the current site was
  // changed by the user via user bypass.
  bool HasUserChangedCookieBlockingForSite();
  void SetUserChangedCookieBlockingForSite(bool changed);

  void AddObserver(CookieControlsObserver* obs);
  void RemoveObserver(CookieControlsObserver* obs);

  base::WeakPtr<CookieControlsController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  struct Status {
    Status(bool controls_visible,
           bool protections_on,
           CookieControlsEnforcement enforcement,
           CookieBlocking3pcdStatus blocking_status,
           base::Time expiration,
           std::vector<TrackingProtectionFeature> features);
    ~Status();
    bool controls_visible;
    bool protections_on;
    CookieControlsEnforcement enforcement;
    CookieBlocking3pcdStatus blocking_status;
    base::Time expiration;
    std::vector<TrackingProtectionFeature> features;
  };

  // The observed WebContents changes during the lifetime of the
  // CookieControlsController. SiteDataObserver can't change the observed
  // object, so we need an inner class that can be recreated when necessary.
  // TODO(dullweber): Make it possible to change the observed class and maybe
  // convert SiteDataObserver to a pure virtual interface.
  class TabObserver
      : public content_settings::PageSpecificContentSettings::SiteDataObserver,
        public content::WebContentsObserver,
        public fingerprinting_protection_filter::
            FingerprintingProtectionObserver {
   public:
    TabObserver(CookieControlsController* cookie_controls,
                content::WebContents* web_contents);

    TabObserver(const TabObserver&) = delete;
    TabObserver& operator=(const TabObserver&) = delete;
    ~TabObserver() override;

    void WebContentsDestroyed() override;

    // PageSpecificContentSettings::SiteDataObserver:
    void OnSiteDataAccessed(const AccessDetails& access_details) override;
    void OnStatefulBounceDetected() override;

    // content::WebContentsObserver:
    void PrimaryPageChanged(content::Page& page) override;
    void DidStopLoading() override;

    // fingerprinting_protection_filter::FingerprintingProtectionObserver:
    void OnSubresourceBlocked() override;

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

    base::ScopedObservation<
        fingerprinting_protection_filter::
            FingerprintingProtectionWebContentsHelper,
        fingerprinting_protection_filter::FingerprintingProtectionObserver>
        fpf_observation_{this};
  };

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;
  void OnCookieSettingChanged() override;

  Status GetStatus(content::WebContents* web_contents);

  std::vector<TrackingProtectionFeature> CreateTrackingProtectionFeatureList(
      CookieControlsEnforcement enforcement,
      bool cookies_allowed,
      bool protections_on);

  CookieControlsEnforcement GetEnforcementForThirdPartyCookieBlocking(
      CookieBlocking3pcdStatus status,
      const GURL url,
      SettingInfo info,
      bool cookies_allowed);

  bool ShowIpProtection() const;
  bool ShowFingerprintingProtection() const;

  bool HasOriginSandboxedTopLevelDocument() const;

  // Updates user bypass visibility and/or highlighting.
  void UpdateUserBypass();

  void UpdateLastVisitedSitesMap();

  void UpdatePageReloadStatus(int recent_reloads_count);

  void OnPageFinishedLoading();

  // Returns the number of stateful bounces leading to this page.
  int GetStatefulBounceCount() const;

  // Returns whether at least one subresource has been blocked on this page.
  bool GetIsSubresourceBlocked() const;

  // Returns the number of allowed third-party sites with cookies.
  int GetAllowedThirdPartyCookiesSitesCount() const;

  // Returns the number of blocked third-party sites with cookies.
  int GetBlockedThirdPartyCookiesSitesCount() const;

  double GetSiteEngagementScore();

  // Record metrics when third-party cookies are allowed.
  void RecordActivationMetrics();

  bool SiteDataAccessed(int third_party_allowed_sites,
                        int third_party_blocked_sites);

  bool ShouldHighlightUserBypass();
  bool ShouldUserBypassIconBeVisible(
      std::vector<TrackingProtectionFeature> features,
      bool protections_on,
      bool controls_visible);
  content::WebContents* GetWebContents() const;

  std::unique_ptr<TabObserver> tab_observer_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Cookie_settings for the original profile associated with
  // |cookie_settings_|, if there is one. For example, in Chrome, this
  // corresponds to the regular profile when |cookie_settings_| is incognito.
  // This may be null.
  scoped_refptr<content_settings::CookieSettings> original_cookie_settings_;
  raw_ptr<HostContentSettingsMap> settings_map_;
  // TrackingProtectionSettings class for the current profile. Corresponds to
  // the regular profile if in incognito, since TP settings should still apply.
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;

  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_observation_{this};

  bool should_reload_ = false;
  bool user_changed_cookie_blocking_ = false;

  // The number of page reloads in last 30 seconds.
  int recent_reloads_count_ = 0;

  bool has_exception_expired_since_last_visit_ = false;

  bool waiting_for_page_load_finish_ = false;

  base::ObserverList<CookieControlsObserver> observers_;

  base::WeakPtrFactory<CookieControlsController> weak_ptr_factory_{this};
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_
