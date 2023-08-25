// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/third_party_site_data_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using base::UserMetricsAction;
using site_engagement::SiteEngagementService;

namespace {

constexpr char kEntryPointAnimatedKey[] = "entry_point_animated";
constexpr char kLastExpirationKey[] = "last_expiration";
constexpr char kLastVisitedActiveException[] = "last_visited_active_exception";
constexpr char kActivationsCountKey[] = "activations_count_key";

base::Value::Dict GetMetadata(HostContentSettingsMap* settings_map,
                              const GURL& url) {
  base::Value stored_value = settings_map->GetWebsiteSetting(
      url, url, ContentSettingsType::COOKIE_CONTROLS_METADATA);
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

int GetActivationCount(const base::Value::Dict& metadata) {
  return metadata.FindInt(kActivationsCountKey).value_or(0);
}

bool HasExceptionExpiredSinceLastVisit(const base::Value::Dict& metadata) {
  auto last_expiration = base::ValueToTime(metadata.Find(kLastExpirationKey))
                             .value_or(base::Time());
  auto last_visited =
      base::ValueToTime(metadata.Find(kLastVisitedActiveException))
          .value_or(base::Time());

  return !last_expiration.is_null()  // Exception should have an expiration,
         && last_expiration < base::Time::Now()  // that has already expired,
         && !last_visited.is_null()              // from a previous visit,
         && last_visited < last_expiration;      // with no visit since.
}

void ApplyMetadataChanges(HostContentSettingsMap* settings_map,
                          const GURL& url,
                          base::Value::Dict&& dict) {
  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::COOKIE_CONTROLS_METADATA,
      base::Value(std::move(dict)));
}

ThirdPartySiteDataAccessType GetSiteDataAccessType(int allowed_sites,
                                                   int blocked_sites) {
  if (blocked_sites > 0) {
    return ThirdPartySiteDataAccessType::kAnyBlockedThirdPartySiteAccesses;
  }
  if (allowed_sites > 0) {
    return ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses;
  }
  return ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses;
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
    ResetInitialCookieControlsStatus();
  }
  auto status = GetStatus(web_contents);
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    int third_party_allowed_sites = GetAllowedThirdPartyCookiesSitesCount();
    int third_party_blocked_sites = GetBlockedThirdPartyCookiesSitesCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : observers_) {
      observer.OnStatusChanged(status.status, status.enforcement,
                               status.expiration);
      observer.OnSitesCountChanged(third_party_allowed_sites,
                                   third_party_blocked_sites);
      observer.OnBreakageConfidenceLevelChanged(
          GetConfidenceLevel(status.status, third_party_allowed_sites,
                             third_party_blocked_sites, bounce_count));
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

  const bool is_default_setting =
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();

  // The UI can reset only host-scoped (without wildcards in the domain) or
  // site-scoped exceptions.
  const bool host_or_site_scoped_exception =
      !info.secondary_pattern.HasDomainWildcard() ||
      info.secondary_pattern == URLToSchemefulSitePattern(url);

  // Rules from regular mode can't be temporarily overridden in incognito.
  bool exception_exists_in_regular_profile = false;
  if (is_allowed && original_cookie_settings_) {
    SettingInfo original_info;
    original_cookie_settings_->IsThirdPartyAccessAllowed(url, &original_info);

    exception_exists_in_regular_profile =
        original_info.primary_pattern != ContentSettingsPattern::Wildcard() ||
        original_info.secondary_pattern != ContentSettingsPattern::Wildcard();
  }

  CookieControlsStatus status = is_allowed
                                    ? CookieControlsStatus::kDisabledForSite
                                    : CookieControlsStatus::kEnabled;
  CookieControlsEnforcement enforcement;
  if (info.source == SETTING_SOURCE_POLICY) {
    enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  } else if (info.source == SETTING_SOURCE_EXTENSION) {
    enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  } else if (exception_exists_in_regular_profile ||
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
                                             int blocked_sites,
                                             int bounce_count) {
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

  // If no 3P sites have attempted to access site data, nor were any stateful
  // bounces recorded, return low confidence. Take into account both allow and
  // blocked counts, since the breakage might be related to storage
  // partitioning. Partitioned site will be allowed to access partitioned
  // storage.
  if (allowed_sites + blocked_sites + bounce_count == 0) {
    return CookieControlsBreakageConfidenceLevel::kLow;
  }

  // TODO(crbug.com/1446230): Check if FedCM was requested.
  auto* web_contents = GetWebContents();
  const GURL& url = web_contents->GetLastCommittedURL();
  if (cookie_settings_->HasAnyFrameRequestedStorageAccess(url)) {
    return CookieControlsBreakageConfidenceLevel::kMedium;
  }

  // If the user is returning to the site after their previous exception has
  // expired, return high confidence. The order of this check is important,
  // as the site may now be using SAA / FedCM instead of relying on 3PC. It
  // should also come before any check for whether the entrypoint was already
  // animated.
  if (has_exception_expired_since_last_visit_) {
    return CookieControlsBreakageConfidenceLevel::kHigh;
  }

  // Check if the entry point was already animated for the site and return
  // medium confidence in that case.
  if (WasEntryPointAlreadyAnimated(GetMetadata(settings_map_, url))) {
    return CookieControlsBreakageConfidenceLevel::kMedium;
  }

  if (recent_reloads_count_ >= features::kUserBypassUIReloadCount.Get()) {
    return CookieControlsBreakageConfidenceLevel::kHigh;
  }

  if (SiteEngagementService::IsEngagementAtLeast(
          GetSiteEngagementScore(), blink::mojom::EngagementLevel::HIGH)) {
    return CookieControlsBreakageConfidenceLevel::kHigh;
  }

  // Default to a medium confidence level, as by this point the site has
  // accessed 3P storage, but there is no signal that would give us high
  // confidence.
  return CookieControlsBreakageConfidenceLevel::kMedium;
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  if (block_third_party_cookies) {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOn"));
    should_reload_ =
        base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI);
    cookie_settings_->ResetThirdPartyCookieSetting(url);
    return;
  }

  CHECK(!block_third_party_cookies);
  base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOff"));
  should_reload_ = true;

  if (!base::FeatureList::IsEnabled(
          content_settings::features::kUserBypassUI)) {
    cookie_settings_->SetThirdPartyCookieSetting(
        url, ContentSetting::CONTENT_SETTING_ALLOW);
    return;
  }

  CHECK(
      base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI));
  cookie_settings_->SetCookieSettingForUserBypass(url);

  // Record expiration metadata for the newly created exception, and increased
  // the activation count.
  base::Value::Dict metadata = GetMetadata(settings_map_, url);
  metadata.Set(kLastExpirationKey,
               base::TimeToValue(GetStatus(GetWebContents()).expiration));
  metadata.Set(kActivationsCountKey, GetActivationCount(metadata) + 1);
  ApplyMetadataChanges(settings_map_, url, std::move(metadata));

  RecordActivationMetrics();
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

CookieControlsBreakageConfidenceLevel
CookieControlsController::GetBreakageConfidenceLevel() {
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    auto status = GetStatus(GetWebContents());
    int allowed_sites = GetAllowedSitesCount();
    int blocked_sites = GetBlockedSitesCount();
    int bounce_count = GetStatefulBounceCount();
    return GetConfidenceLevel(status.status, allowed_sites, blocked_sites,
                              bounce_count);
  } else {
    return CookieControlsBreakageConfidenceLevel::kUninitialized;
  }
}

CookieControlsStatus CookieControlsController::GetCookieControlsStatus() {
  auto status = GetStatus(GetWebContents());
  return status.status;
}

int CookieControlsController::GetAllowedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (pscs) {
    return pscs->allowed_local_shared_objects().GetObjectCount() +
           pscs->allowed_browsing_data_model()->size();
  } else {
    return 0;
  }
}
int CookieControlsController::GetBlockedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (pscs) {
    return pscs->blocked_local_shared_objects().GetObjectCount() +
           pscs->blocked_browsing_data_model()->size();
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

int CookieControlsController::GetAllowedThirdPartyCookiesSitesCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }

  return browsing_data::GetUniqueThirdPartyCookiesHostCount(
      GetWebContents()->GetLastCommittedURL(),
      pscs->allowed_local_shared_objects(),
      *(pscs->allowed_browsing_data_model()));
}

int CookieControlsController::GetBlockedThirdPartyCookiesSitesCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }

  return browsing_data::GetUniqueThirdPartyCookiesHostCount(
      GetWebContents()->GetLastCommittedURL(),
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
    int third_party_allowed_sites = GetAllowedThirdPartyCookiesSitesCount();
    int third_party_blocked_sites = GetBlockedThirdPartyCookiesSitesCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : observers_) {
      observer.OnSitesCountChanged(third_party_allowed_sites,
                                   third_party_blocked_sites);
      observer.OnBreakageConfidenceLevelChanged(
          GetConfidenceLevel(status.status, third_party_allowed_sites,
                             third_party_blocked_sites, bounce_count));
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

  ResetInitialCookieControlsStatus();
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kUserBypassUI)) {
    return;
  }

  // Cache whether the expiration has expired since last visit before updating
  // the last visited metadata.
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  has_exception_expired_since_last_visit_ =
      HasExceptionExpiredSinceLastVisit(GetMetadata(settings_map_, url));

  // We only care about visits with active expirations, if there is an active
  // exception, update the last visited time, otherwise clear it.
  base::Value::Dict metadata = GetMetadata(settings_map_, url);
  auto status = GetStatus(GetWebContents());
  if (status.status == CookieControlsStatus::kDisabledForSite) {
    metadata.Set(kLastVisitedActiveException,
                 base::TimeToValue(base::Time::Now()));
  } else {
    metadata.Remove(kLastVisitedActiveException);
  }
  ApplyMetadataChanges(settings_map_, url, std::move(metadata));

  recent_reloads_count_ = recent_reloads_count;

  // Only inform the observers if there is a potential confidence level change.
  if (recent_reloads_count_ < features::kUserBypassUIReloadCount.Get() &&
      !has_exception_expired_since_last_visit_) {
    return;
  }

  int allowed_sites = GetAllowedSitesCount();
  int blocked_sites = GetBlockedSitesCount();
  int bounce_count = GetStatefulBounceCount();

  for (auto& observer : observers_) {
    observer.OnBreakageConfidenceLevelChanged(GetConfidenceLevel(
        status.status, allowed_sites, blocked_sites, bounce_count));
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

double CookieControlsController::GetSiteEngagementScore() {
  auto* web_contents = GetWebContents();
  return SiteEngagementService::Get(web_contents->GetBrowserContext())
      ->GetScore(web_contents->GetVisibleURL());
}

void CookieControlsController::RecordActivationMetrics() {
  const GURL& url = GetWebContents()->GetLastCommittedURL();

  // Metrics, related to confidence signals:
  // TODO(crbug.com/1446230): Add CookieControlsActivated.FedCmInitiated
  base::UmaHistogramBoolean(
      "Privacy.CookieControlsActivated.SaaRequested",
      cookie_settings_->HasAnyFrameRequestedStorageAccess(url));
  base::UmaHistogramCounts100(
      "Privacy.CookieControlsActivated.PageRefreshCount",
      recent_reloads_count_);
  base::UmaHistogramExactLinear(
      "Privacy.CookieControlsActivated.SiteEngagementScore",
      GetSiteEngagementScore(), 100);

  int allowed_sites = GetAllowedSitesCount();
  int blocked_sites = GetBlockedSitesCount();
  auto site_data_access_type =
      GetSiteDataAccessType(allowed_sites, blocked_sites);
  base::UmaHistogramEnumeration(
      "Privacy.CookieControlsActivated.SiteDataAccessType",
      site_data_access_type);

  // Record activation UKM.
  // TODO(crbug.com/1446230): Include FedCM information.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto ukm_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::ThirdPartyCookies_CookieControlsActivated(ukm_source_id)
      .SetFedCmInitiated(false)
      .SetStorageAccessAPIRequested(
          cookie_settings_->HasAnyFrameRequestedStorageAccess(url))
      .SetPageRefreshCount(std::clamp(recent_reloads_count_, 0, 10))
      .SetRepeatedActivation(
          GetActivationCount(GetMetadata(settings_map_, url)) > 1)
      .SetSiteEngagementLevel(static_cast<uint64_t>(
          SiteEngagementService::Get(GetWebContents()->GetBrowserContext())
              ->GetEngagementLevel(url)))
      .SetThirdPartySiteDataAccessType(
          static_cast<uint64_t>(site_data_access_type))
      .Record(ukm::UkmRecorder::Get());

  // TODO(crbug.com/1446230): Add metrics, related to repeated activations.
}

void CookieControlsController::ResetInitialCookieControlsStatus() {
  auto status = GetStatus(GetWebContents());
  initial_page_cookie_controls_status_ = status.status;
  CHECK(initial_page_cookie_controls_status_ !=
        CookieControlsStatus::kUninitialized);
}

CookieControlsController::TabObserver::TabObserver(
    CookieControlsController* cookie_controls,
    content::WebContents* web_contents)
    : content_settings::PageSpecificContentSettings::SiteDataObserver(
          web_contents),
      content::WebContentsObserver(web_contents),
      cookie_controls_(cookie_controls) {
  last_visited_url_ =
      content::WebContentsObserver::web_contents()->GetVisibleURL();
}

CookieControlsController::TabObserver::~TabObserver() = default;

void CookieControlsController::TabObserver::OnSiteDataAccessed(
    const AccessDetails& access_details) {
  if (access_details.site_data_type != SiteDataType::kCookies ||
      !base::FeatureList::IsEnabled(
          content_settings::features::kUserBypassUI)) {
    cookie_controls_->PresentBlockedCookieCounter();
    return;
  }

  // When User Bypass is enabled, a large number of string comparisons are
  // performed to determine what sites are 3P / 1P. Cookie accesses are
  // reported _very_ frequently as many sites are always reading or writing to
  // the same cookie, and there is no caching of these accesses anywhere before
  // here (in constrast to JS storage, which does cache accesses earlier).
  // A simple cache of cookie accesses is used here to limit the number of
  // repeated updates.
  // We can't cache all types of accesses here, because the `site_data_type` is
  // not always populated with sufficient granularity (often aliasing to
  // kUnknown). This is relevant as some daya types may impact the block 3P
  // count, while others may not.
  // TODO(crbug.com/1271155): Replace the SiteDataType with the Browsing Data
  // Model's StorageType, which would let us remove an enum, and let us cache
  // all accesses here.

  if (cookie_accessed_set_.count(access_details)) {
    return;
  }

  cookie_accessed_set_.insert(access_details);
  cookie_controls_->PresentBlockedCookieCounter();
}

void CookieControlsController::TabObserver::OnStatefulBounceDetected() {
  cookie_controls_->PresentBlockedCookieCounter();
}

void CookieControlsController::TabObserver::PrimaryPageChanged(
    content::Page& page) {
  const GURL& current_url =
      content::WebContentsObserver::web_contents()->GetVisibleURL();
  cookie_accessed_set_.clear();

  if (current_url != last_visited_url_) {
    reload_count_ = 0;
    timer_.Stop();
  } else {
    if (!timer_.IsRunning()) {
      timer_.Start(FROM_HERE, features::kUserBypassUIReloadTime.Get(), this,
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
