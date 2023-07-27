// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/site_for_cookies.h"

using base::UserMetricsAction;
using site_engagement::SiteEngagementService;

namespace {

// The number of page reloads in the last 30 seconds that is considered to be a
// high confidence breakage signal.
constexpr int kFrequentReloadThreshold = 3;

constexpr char kEntryPointAnimatedKey[] = "entry_point_animated";

base::Value::Dict GetMetadata(HostContentSettingsMap* settings_map,
                              const GURL& url) {
  base::Value stored_value = settings_map->GetWebsiteSetting(
      url, url, ContentSettingsType::COOKIE_CONTROLS_METADATA, nullptr);
  if (!stored_value.is_dict()) {
    return base::Value::Dict();
  }

  return std::move(stored_value.GetDict());
}

bool WasEntryPointAlreadyAnimated(const base::Value::Dict& metadata) {
  absl::optional<bool> entry_point_animated =
      metadata.FindBool(kEntryPointAnimatedKey);
  return entry_point_animated.has_value() && entry_point_animated.value();
}

void ApplyMetadataChanges(HostContentSettingsMap* settings_map,
                          const GURL& url,
                          base::Value::Dict&& dict) {
  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::COOKIE_CONTROLS_METADATA,
      base::Value(std::move(dict)));
}

}  // namespace

namespace content_settings {

CookieControlsController::CookieControlsController(
    scoped_refptr<CookieSettings> cookie_settings,
    scoped_refptr<CookieSettings> original_cookie_settings,
    HostContentSettingsMap* settings_map)
    : cookie_settings_(cookie_settings),
      original_cookie_settings_(original_cookie_settings),
      settings_map_(settings_map) {
  cookie_observation_.Observe(cookie_settings_.get());
}

CookieControlsController::~CookieControlsController() = default;

void CookieControlsController::OnUiClosing() {
  auto* web_contents = GetWebContents();
  if (should_reload_ && web_contents && !web_contents->IsBeingDestroyed()) {
    web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  }
  should_reload_ = false;
}

void CookieControlsController::Update(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!tab_observer_ || GetWebContents() != web_contents) {
    tab_observer_ = std::make_unique<TabObserver>(this, web_contents);
  }
  auto status = GetStatus(web_contents);
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    int allowed_sites = GetAllowedSitesCount();
    int blocked_sites = GetBlockedSitesCount();

    for (auto& observer : observers_) {
      observer.OnStatusChanged(status.status, status.enforcement,
                               status.expiration);
      observer.OnSitesCountChanged(allowed_sites, blocked_sites);
      observer.OnBreakageConfidenceLevelChanged(
          GetConfidenceLevel(status.status, allowed_sites, blocked_sites));
    }
  } else {
    int allowed_cookies = GetAllowedCookieCount();
    int blocked_cookies = GetBlockedCookieCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : old_observers_) {
      observer.OnStatusChanged(status.status, status.enforcement,
                               allowed_cookies, blocked_cookies);
      observer.OnStatefulBounceCountChanged(bounce_count);
    }
  }
}

CookieControlsController::Status CookieControlsController::GetStatus(
    content::WebContents* web_contents) {
  if (!cookie_settings_->ShouldBlockThirdPartyCookies()) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement, base::Time()};
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(kExtensionScheme)) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement, base::Time()};
  }

  SettingInfo info;
  bool is_allowed = cookie_settings_->IsThirdPartyAccessAllowed(url, &info);

  const bool is_default_setting = info.primary_pattern.MatchesAllHosts() &&
                                  info.secondary_pattern.MatchesAllHosts();

  // The UI can reset only host-scoped (without wildcards in the domain) or
  // site-scoped exceptions.
  const bool host_or_site_scoped_exception =
      !info.secondary_pattern.HasDomainWildcard() ||
      info.secondary_pattern == ContentSettingsPattern::FromURL(url);

  // Rules from regular mode can't be temporarily overridden in incognito.
  const bool is_exception_from_regular_mode_in_incognito =
      is_allowed && original_cookie_settings_ &&
      original_cookie_settings_->ShouldBlockThirdPartyCookies() &&
      original_cookie_settings_->IsThirdPartyAccessAllowed(url,
                                                           nullptr /* info */);

  CookieControlsStatus status = is_allowed
                                    ? CookieControlsStatus::kDisabledForSite
                                    : CookieControlsStatus::kEnabled;
  CookieControlsEnforcement enforcement;
  if (info.source == SETTING_SOURCE_POLICY) {
    enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  } else if (info.source == SETTING_SOURCE_EXTENSION) {
    enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  } else if (is_exception_from_regular_mode_in_incognito ||
             (!is_default_setting && !host_or_site_scoped_exception)) {
    // If the exception cannot be reset in-context because of the nature of the
    // setting, display as managed by setting.
    enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  } else {
    enforcement = CookieControlsEnforcement::kNoEnforcement;
  }
  return {status, enforcement, info.metadata.expiration()};
}

CookieControlsBreakageConfidenceLevel
CookieControlsController::GetConfidenceLevel(CookieControlsStatus status,
                                             int allowed_sites,
                                             int blocked_sites) {
  // If 3PC cookies are not blocked by default:
  switch (status) {
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kUninitialized:
      return CookieControlsBreakageConfidenceLevel::kUninitialized;
    case CookieControlsStatus::kDisabledForSite:
      return CookieControlsBreakageConfidenceLevel::kMedium;
    case CookieControlsStatus::kEnabled:
      // Check other conditions to determine the level.
      break;
  }

  // TODO(crbug.com/1446230): Check if the exception has expired since the last
  // page visit.

  // If no 3P sites have attempted to access site data:
  // (taking into account both allow and blocked counts, since the breakage
  // might be related to storage partitioning. Partitioned site will be allowed
  // to access partitioned storage)
  if (allowed_sites + blocked_sites == 0) {
    return CookieControlsBreakageConfidenceLevel::kLow;
  }

  // TODO(crbug.com/1446230): Check if FedCM was requested.
  auto* web_contents = GetWebContents();
  const GURL& url = web_contents->GetLastCommittedURL();
  if (cookie_settings_->HasAnyFrameRequestedStorageAccess(url)) {
    return CookieControlsBreakageConfidenceLevel::kMedium;
  }

  // Only high confidence signals from now on. Check if the entry point was
  // already animated for the site and return medium confidence in that case.
  if (WasEntryPointAlreadyAnimated(GetMetadata(settings_map_, url))) {
    return CookieControlsBreakageConfidenceLevel::kMedium;
  }

  if (recent_reloads_count_ >= kFrequentReloadThreshold) {
    return CookieControlsBreakageConfidenceLevel::kHigh;
  }

  auto score = SiteEngagementService::Get(GetWebContents()->GetBrowserContext())
                   ->GetScore(GetWebContents()->GetVisibleURL());
  if (SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::HIGH)) {
    return CookieControlsBreakageConfidenceLevel::kHigh;
  }

  return CookieControlsBreakageConfidenceLevel::kMedium;
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  if (block_third_party_cookies) {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOn"));
    should_reload_ = false;
    cookie_settings_->ResetThirdPartyCookieSetting(
        GetWebContents()->GetLastCommittedURL());
  } else {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOff"));
    should_reload_ = true;
    if (base::FeatureList::IsEnabled(
            content_settings::features::kUserBypassUI)) {
      cookie_settings_->SetCookieSettingForUserBypass(
          GetWebContents()->GetLastCommittedURL());
    } else {
      cookie_settings_->SetThirdPartyCookieSetting(
          GetWebContents()->GetLastCommittedURL(),
          ContentSetting::CONTENT_SETTING_ALLOW);
    }
  }
}

void CookieControlsController::OnEntryPointAnimated() {
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  base::Value::Dict metadata = GetMetadata(settings_map_, url);
  metadata.Set(kEntryPointAnimatedKey, base::Value(true));
  ApplyMetadataChanges(settings_map_, url, std::move(metadata));
}

bool CookieControlsController::FirstPartyCookiesBlocked() {
  // No overrides are given since existing ones only pertain to 3P checks.
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  return !cookie_settings_->IsFullCookieAccessAllowed(
      url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
      net::CookieSettingOverrides());
}

bool CookieControlsController::HasCookieBlockingChangedForSite() {
  auto current_status = GetStatus(GetWebContents());
  return current_status.status != initial_page_cookie_controls_status_;
}

int CookieControlsController::GetAllowedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (pscs) {
    return pscs->allowed_local_shared_objects().GetObjectCount();
  } else {
    return 0;
  }
}
int CookieControlsController::GetBlockedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (pscs) {
    return pscs->blocked_local_shared_objects().GetObjectCount();
  } else {
    return 0;
  }
}

int CookieControlsController::GetAllowedSitesCount() const {
  // TODO(crbug.com/1446230): The method should return the number of allowed
  // *third-party* sites (and take BDM into account).
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }
  return browsing_data::GetUniqueHostCount(
      pscs->allowed_local_shared_objects(),
      *(pscs->allowed_browsing_data_model()));
}

int CookieControlsController::GetBlockedSitesCount() const {
  // TODO(crbug.com/1446230): The method should return the number of blocked
  // *third-party* sites (and take BDM into account).
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }
  return browsing_data::GetUniqueHostCount(
      pscs->blocked_local_shared_objects(),
      *(pscs->blocked_browsing_data_model()));
}

int CookieControlsController::GetStatefulBounceCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (pscs) {
    return pscs->stateful_bounce_count();
  } else {
    return 0;
  }
}

void CookieControlsController::PresentBlockedCookieCounter() {
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    auto status = GetStatus(GetWebContents());
    int allowed_sites = GetAllowedSitesCount();
    int blocked_sites = GetBlockedSitesCount();

    for (auto& observer : observers_) {
      observer.OnSitesCountChanged(allowed_sites, blocked_sites);
      observer.OnBreakageConfidenceLevelChanged(
          GetConfidenceLevel(status.status, allowed_sites, blocked_sites));
    }
  } else {
    int allowed_cookies = GetAllowedCookieCount();
    int blocked_cookies = GetBlockedCookieCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : old_observers_) {
      observer.OnCookiesCountChanged(allowed_cookies, blocked_cookies);
      observer.OnStatefulBounceCountChanged(bounce_count);
    }
  }
}

void CookieControlsController::OnPageReloadDetected(int recent_reloads_count) {
  if (HasCookieBlockingChangedForSite() && recent_reloads_count > 0) {
    waiting_for_page_load_finish_ = true;
  }

  auto status = GetStatus(GetWebContents());
  initial_page_cookie_controls_status_ = status.status;
  CHECK(initial_page_cookie_controls_status_ !=
        CookieControlsStatus::kUninitialized);

  if (!base::FeatureList::IsEnabled(
          content_settings::features::kUserBypassUI)) {
    return;
  }

  recent_reloads_count_ = recent_reloads_count;
  // Only inform the observers if the reload count is higher than the threshold.
  if (recent_reloads_count_ < kFrequentReloadThreshold) {
    return;
  }

  int allowed_sites = GetAllowedSitesCount();
  int blocked_sites = GetBlockedSitesCount();

  for (auto& observer : observers_) {
    observer.OnBreakageConfidenceLevelChanged(
        GetConfidenceLevel(status.status, allowed_sites, blocked_sites));
  }
}

void CookieControlsController::OnPageFinishedLoading() {
  if (!waiting_for_page_load_finish_) {
    return;
  }
  waiting_for_page_load_finish_ = false;

  for (auto& observer : observers_) {
    observer.OnFinishedPageReloadWithChangedSettings();
  }
}

void CookieControlsController::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  if (GetWebContents()) {
    Update(GetWebContents());
  }
}

void CookieControlsController::OnCookieSettingChanged() {
  if (GetWebContents()) {
    Update(GetWebContents());
  }
}

content::WebContents* CookieControlsController::GetWebContents() const {
  if (!tab_observer_) {
    return nullptr;
  }
  return tab_observer_->content::WebContentsObserver::web_contents();
}

void CookieControlsController::AddObserver(OldCookieControlsObserver* obs) {
  old_observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(OldCookieControlsObserver* obs) {
  old_observers_.RemoveObserver(obs);
}

void CookieControlsController::AddObserver(CookieControlsObserver* obs) {
  observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(CookieControlsObserver* obs) {
  observers_.RemoveObserver(obs);
}

CookieControlsController::TabObserver::TabObserver(
    CookieControlsController* cookie_controls,
    content::WebContents* web_contents)
    : content_settings::PageSpecificContentSettings::SiteDataObserver(
          web_contents),
      content::WebContentsObserver(web_contents),
      cookie_controls_(cookie_controls) {}

void CookieControlsController::TabObserver::OnSiteDataAccessed(
    const AccessDetails& access_details) {
  cookie_controls_->PresentBlockedCookieCounter();
}

void CookieControlsController::TabObserver::OnStatefulBounceDetected() {
  cookie_controls_->PresentBlockedCookieCounter();
}

void CookieControlsController::TabObserver::PrimaryPageChanged(
    content::Page& page) {
  const GURL& current_url =
      content::WebContentsObserver::web_contents()->GetVisibleURL();

  if (current_url != last_visited_url_) {
    reload_count_ = 0;
    timer_.Stop();
  } else {
    if (!timer_.IsRunning()) {
      timer_.Start(FROM_HERE, base::Seconds(30), this,
                   &CookieControlsController::TabObserver::ResetReloadCounter);
    }
    reload_count_++;
  }
  last_visited_url_ = current_url;
  cookie_controls_->OnPageReloadDetected(reload_count_);
}

void CookieControlsController::TabObserver::DidStopLoading() {
  cookie_controls_->OnPageFinishedLoading();
}

void CookieControlsController::TabObserver::ResetReloadCounter() {
  reload_count_ = 0;
}

}  // namespace content_settings
