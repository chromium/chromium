// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/third_party_site_data_access_type.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/ip_protection/common/ip_protection_status.h"
#include "components/ip_protection/common/ip_protection_status_observer.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::base::UserMetricsAction;
using ::site_engagement::SiteEngagementService;

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
  std::optional<bool> entry_point_animated =
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
    HostContentSettingsMap* settings_map,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito_profile)
    : cookie_settings_(cookie_settings),
      original_cookie_settings_(original_cookie_settings),
      settings_map_(settings_map),
      tracking_protection_settings_(tracking_protection_settings),
      is_incognito_profile_(is_incognito_profile) {
  CHECK(cookie_settings_);
  CHECK(tracking_protection_settings_);
  cookie_observation_.Observe(cookie_settings_.get());
}

CookieControlsController::Status::Status(
    CookieControlsState controls_state,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration)
    : controls_state(controls_state),
      enforcement(enforcement),
      blocking_status(blocking_status),
      expiration(expiration) {}
CookieControlsController::Status::~Status() = default;

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
    SetUserChangedCookieBlockingForSite(false);
  }
  if (observers_.empty()) {
    return;
  }
  auto status = GetStatus(web_contents);
  const bool icon_visible =
      ShouldUserBypassIconBeVisible(status.controls_state);
  const bool should_highlight =
      ShouldHighlightUserBypass(status.controls_state);
  for (auto& observer : observers_) {
    observer.OnStatusChanged(status.controls_state, status.enforcement,
                             status.blocking_status, status.expiration);
    observer.OnCookieControlsIconStatusChanged(
        icon_visible, status.controls_state, status.blocking_status,
        should_highlight);
  }
}

void CookieControlsController::OnSubresourceBlocked() {
  // When a subresource is blocked by fingerprinting protection,
  // `UpdateUserBypass` will show the User Bypass.
  UpdateUserBypass();
}

void CookieControlsController::OnFirstSubresourceProxiedOnCurrentPrimaryPage() {
  UpdateUserBypass();
}

CookieControlsController::Status CookieControlsController::GetStatus(
    content::WebContents* web_contents) {
  if (!cookie_settings_->ShouldBlockThirdPartyCookies()) {
    return {CookieControlsState::kHidden,
            CookieControlsEnforcement::kNoEnforcement,
            CookieBlocking3pcdStatus::kNotIn3pcd, base::Time()};
  }

  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(kExtensionScheme)) {
    return {CookieControlsState::kHidden,
            CookieControlsEnforcement::kNoEnforcement,
            CookieBlocking3pcdStatus::kNotIn3pcd, base::Time()};
  }

  auto blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;
  if (cookie_settings_->AreThirdPartyCookiesLimited()) {
    blocking_status = CookieBlocking3pcdStatus::kLimited;
  } else if (tracking_protection_settings_->AreAllThirdPartyCookiesBlocked()) {
    blocking_status = CookieBlocking3pcdStatus::kAll;
  }

  SettingInfo info;
  bool cookies_allowed =
      cookie_settings_->IsThirdPartyAccessAllowed(url, &info);
  CookieControlsEnforcement enforcement =
      GetEnforcementForThirdPartyCookieBlocking(blocking_status, url, info,
                                                cookies_allowed);

  CookieControlsState controls_state;
  if (enforcement == CookieControlsEnforcement::kEnforcedByTpcdGrant) {
    controls_state = CookieControlsState::kHidden;
  } else if (ShowActFeatures()) {
    controls_state =
        tracking_protection_settings_->HasTrackingProtectionException(url,
                                                                      &info)
            ? CookieControlsState::kPausedTp
            : CookieControlsState::kActiveTp;
  } else {
    controls_state = cookies_allowed ? CookieControlsState::kAllowed3pc
                                     : CookieControlsState::kBlocked3pc;
  }

  return {controls_state, enforcement, blocking_status,
          info.metadata.expiration()};
}

void CookieControlsController::RecordActMetrics(bool pause_protections) {
  if (GetIsSubresourceBlocked()) {
    base::RecordAction(UserMetricsAction(
        pause_protections
            ? "TrackingProtections.Bubble.FppActive.DisableProtections"
            : "TrackingProtections.Bubble.FppActive.EnableProtections"));
  }
  if (GetIsSubresourceProxied()) {
    base::RecordAction(UserMetricsAction(
        pause_protections
            ? "TrackingProtections.Bubble.IppActive.DisableProtections"
            : "TrackingProtections.Bubble.IppActive.EnableProtections"));
  }
}

bool CookieControlsController::ShowActFeatures() {
  return is_incognito_profile_ &&
         (tracking_protection_settings_->IsIpProtectionEnabled() ||
          tracking_protection_settings_->IsFpProtectionEnabled()) &&
         base::FeatureList::IsEnabled(privacy_sandbox::kActUserBypassUx);
}

CookieControlsEnforcement
CookieControlsController::GetEnforcementForThirdPartyCookieBlocking(
    CookieBlocking3pcdStatus status,
    const GURL url,
    const SettingInfo& info,
    bool cookies_allowed) {
  const bool is_default_setting =
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();

  // The UI can reset only host-scoped (without wildcards in the domain) or
  // site-scoped exceptions.
  const bool host_or_site_scoped_exception =
      !info.secondary_pattern.HasDomainWildcard() ||
      info.secondary_pattern ==
          ContentSettingsPattern::FromURLToSchemefulSitePattern(url);

  // Rules from regular mode can't be temporarily overridden in off the record
  // profiles.
  bool exception_exists_in_regular_profile = false;
  if (cookies_allowed && original_cookie_settings_) {
    SettingInfo original_info;
    original_cookie_settings_->IsThirdPartyAccessAllowed(url, &original_info);

    exception_exists_in_regular_profile =
        original_info.primary_pattern != ContentSettingsPattern::Wildcard() ||
        original_info.secondary_pattern != ContentSettingsPattern::Wildcard();
  }

  if (info.source == SettingSource::kTpcdGrant &&
      status == CookieBlocking3pcdStatus::kLimited) {
    return CookieControlsEnforcement::kEnforcedByTpcdGrant;
  } else if (info.source == SettingSource::kPolicy) {
    return CookieControlsEnforcement::kEnforcedByPolicy;
  } else if (info.source == SettingSource::kExtension) {
    return CookieControlsEnforcement::kEnforcedByExtension;
  } else if (exception_exists_in_regular_profile ||
             (!is_default_setting && !host_or_site_scoped_exception)) {
    // If the exception cannot be reset in-context because of the nature of the
    // setting, display as managed by setting.
    return CookieControlsEnforcement::kEnforcedByCookieSetting;
  } else {
    return CookieControlsEnforcement::kNoEnforcement;
  }
}

bool CookieControlsController::HasOriginSandboxedTopLevelDocument() const {
  content::RenderFrameHost* rfh = GetWebContents()->GetPrimaryMainFrame();
  // If the WebContents has not fully initialized the RenderFrameHost yet.
  // TODO(crbug.com/346386726): Remove the HasPolicyContainerHost() call once
  //   RenderFrameHost initialization order is fixed.
  if (!rfh || !rfh->HasPolicyContainerHost()) {
    // In that case, we fall back on assuming it is not sandboxed.
    // Since this is only for determining whether to render the User Bypass
    // icon this fallback is acceptable.
    return false;
  }

  return rfh->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin);
}

void CookieControlsController::OnTrackingProtectionsChangedForSite(
    bool pause_protections) {
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  if (pause_protections) {
    tracking_protection_settings_->AddTrackingProtectionException(url);
  } else {
    tracking_protection_settings_->RemoveTrackingProtectionException(url);
  }
  OnCookieBlockingEnabledForSite(!pause_protections);
  RecordActMetrics(pause_protections);
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  should_reload_ = true;
  if (block_third_party_cookies) {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOn"));
    cookie_settings_->ResetThirdPartyCookieSetting(url);
    return;
  }

  CHECK(!block_third_party_cookies);
  base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOff"));
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
  // sanity check if WebContents was instantiated (update method called before)
  // TODO(b/341972754): refactor this to be handled properly via update method
  // for all Android corner cases.
  if (GetWebContents() == nullptr) {
    return;
  }
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  base::Value::Dict metadata = GetMetadata(settings_map_, url);
  metadata.Set(kEntryPointAnimatedKey, base::Value(true));
  ApplyMetadataChanges(settings_map_, url, std::move(metadata));
}

bool CookieControlsController::HasUserChangedCookieBlockingForSite() {
  return user_changed_cookie_blocking_;
}

void CookieControlsController::SetUserChangedCookieBlockingForSite(
    bool changed) {
  // Avoid a toggle back and forth being marked as "changed".
  user_changed_cookie_blocking_ = changed && !user_changed_cookie_blocking_;
}

int CookieControlsController::GetAllowedThirdPartyCookiesSitesCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      GetWebContents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }

  return browsing_data::GetUniqueThirdPartyCookiesHostCount(
      GetWebContents()->GetLastCommittedURL(),
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

bool CookieControlsController::GetIsSubresourceBlocked() const {
  // Check WebContents are valid. A possible race condition on Android causes
  // this to be called before WebContents are instantiated.
  if (GetWebContents() == nullptr) {
    return false;
  }
  auto* fpf_web_contents_helper = fingerprinting_protection_filter::
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          GetWebContents());
  return fpf_web_contents_helper != nullptr &&
         fpf_web_contents_helper->subresource_blocked_in_current_primary_page();
}

bool CookieControlsController::GetIsSubresourceProxied() const {
  // Check WebContents are valid. A possible race condition on Android causes
  // this to be called before WebContents are instantiated.
  if (GetWebContents() == nullptr) {
    return false;
  }

  auto* ip_protection_status =
      ip_protection::IpProtectionStatus::FromWebContents(GetWebContents());
  return ip_protection_status != nullptr &&
         ip_protection_status->IsSubresourceProxiedOnCurrentPrimaryPage();
}

void CookieControlsController::UpdateUserBypass() {
  if (observers_.empty()) {
    return;
  }
  auto status = GetStatus(GetWebContents());
  const bool icon_visible =
      ShouldUserBypassIconBeVisible(status.controls_state);
  const bool should_highlight =
      ShouldHighlightUserBypass(status.controls_state);
  for (auto& observer : observers_) {
    observer.OnCookieControlsIconStatusChanged(
        icon_visible, status.controls_state, status.blocking_status,
        should_highlight);
  }
}

void CookieControlsController::UpdateLastVisitedSitesMap() {
  // Cache whether the expiration has expired since last visit before updating
  // the last visited metadata.
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  has_exception_expired_since_last_visit_ =
      HasExceptionExpiredSinceLastVisit(GetMetadata(settings_map_, url));

  // We only care about visits with active expirations, if there is an active
  // exception, update the last visited time, otherwise clear it.
  base::Value::Dict metadata = GetMetadata(settings_map_, url);
  auto status = GetStatus(GetWebContents());
  if (status.controls_state == CookieControlsState::kAllowed3pc) {
    metadata.Set(kLastVisitedActiveException,
                 base::TimeToValue(base::Time::Now()));
  } else {
    metadata.Remove(kLastVisitedActiveException);
  }
  ApplyMetadataChanges(settings_map_, url, std::move(metadata));
}

void CookieControlsController::UpdatePageReloadStatus(
    int recent_reloads_count) {
  if (HasUserChangedCookieBlockingForSite() && recent_reloads_count > 0) {
    waiting_for_page_load_finish_ = true;
  }
  SetUserChangedCookieBlockingForSite(false);
  recent_reloads_count_ = recent_reloads_count;

  if (recent_reloads_count_ >= features::kUserBypassUIReloadCount.Get()) {
    for (auto& observer : observers_) {
      observer.OnReloadThresholdExceeded();
    }
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
  // TODO(crbug.com/40064612): Add CookieControlsActivated.FedCmInitiated
  base::UmaHistogramBoolean(
      "Privacy.CookieControlsActivated.SaaRequested",
      cookie_settings_->HasAnyFrameRequestedStorageAccess(url));
  base::UmaHistogramCounts100(
      "Privacy.CookieControlsActivated.PageRefreshCount",
      recent_reloads_count_);
  base::UmaHistogramExactLinear(
      "Privacy.CookieControlsActivated.SiteEngagementScore",
      GetSiteEngagementScore(), 100);

  auto site_data_access_type =
      GetSiteDataAccessType(GetAllowedThirdPartyCookiesSitesCount(),
                            GetBlockedThirdPartyCookiesSitesCount());
  base::UmaHistogramEnumeration(
      "Privacy.CookieControlsActivated.SiteDataAccessType",
      site_data_access_type);

  // Record activation UKM.
  // TODO(crbug.com/40064612): Include FedCM information.
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

  // TODO(crbug.com/40064612): Add metrics, related to repeated activations.
}

bool CookieControlsController::ShouldHighlightUserBypass(
    CookieControlsState controls_state) {
  // Highlighting is meant to draw attention to bypassing, so just return if
  // bypass has already happened.
  if (controls_state == CookieControlsState::kHidden ||
      controls_state == CookieControlsState::kAllowed3pc) {
    return false;
  }

  auto* web_contents = GetWebContents();
  // We don't want to show UI animation, and IPH in this case as we can't
  // persist their usage cross-session. This puts us at high risk of
  // over-triggering noisy UI and annoying users.
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }

  // TODO(crbug.com/40064612): Check if FedCM was requested.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (cookie_settings_->HasAnyFrameRequestedStorageAccess(url)) {
    return false;
  }

  // If the user is returning to the site after their previous exception has
  // expired, highlight user bypass. The order of this check is important,
  // as the site may now be using SAA / FedCM instead of relying on 3PC. It
  // should also come before any check for whether the entrypoint was already
  // animated.
  if (has_exception_expired_since_last_visit_) {
    return true;
  }

  // Check if the entry point was already animated for the site.
  if (WasEntryPointAlreadyAnimated(GetMetadata(settings_map_, url))) {
    return false;
  }

  if (recent_reloads_count_ >= features::kUserBypassUIReloadCount.Get()) {
    return true;
  }

  if (SiteEngagementService::IsEngagementAtLeast(
          GetSiteEngagementScore(), blink::mojom::EngagementLevel::HIGH)) {
    return true;
  }

  return false;
}

bool CookieControlsController::ShouldUserBypassIconBeVisible(
    CookieControlsState controls_state) {
  if (controls_state == CookieControlsState::kHidden) {
    return false;
  }
  // 3PCD prevents SameSite=None cookies from being sent when the top-level
  // document is sandboxed without `allow-origin`. For instance when loaded
  // with: `Content-Security-Policy: sandbox`. In that case, we render the UI to
  // allow the user to opt into sending SameSite=None cookies again in those
  // contexts.
  return HasOriginSandboxedTopLevelDocument() ||
         controls_state == CookieControlsState::kAllowed3pc ||
         controls_state == CookieControlsState::kPausedTp ||
         // If no 3P sites have attempted to access site data, nor were any
         // stateful bounces recorded, the icon should not be displayed. Take
         // into account both allow and blocked counts, since the breakage might
         // be related to storage partitioning. Partitioned site will be allowed
         // to access partitioned storage.
         SiteDataAccessAttempted() || GetIsSubresourceBlocked() ||
         GetIsSubresourceProxied();
}

bool CookieControlsController::SiteDataAccessAttempted() {
  return GetStatefulBounceCount() || GetAllowedThirdPartyCookiesSitesCount() ||
         GetBlockedThirdPartyCookiesSitesCount();
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
  auto* fpf_web_contents_helper = fingerprinting_protection_filter::
      FingerprintingProtectionWebContentsHelper::FromWebContents(web_contents);
  if (fpf_web_contents_helper) {
    fpf_observation_.Observe(fpf_web_contents_helper);
  }

  auto* ip_protection_status =
      ip_protection::IpProtectionStatus::FromWebContents(web_contents);
  if (ip_protection_status) {
    ip_protection_observation_.Observe(ip_protection_status);
  }
}

CookieControlsController::TabObserver::~TabObserver() = default;

void CookieControlsController::TabObserver::WebContentsDestroyed() {
  fpf_observation_.Reset();
  ip_protection_observation_.Reset();
}

void CookieControlsController::TabObserver::OnSiteDataAccessed(
    const AccessDetails& access_details) {
  if (access_details.site_data_type != SiteDataType::kCookies) {
    cookie_controls_->UpdateUserBypass();
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
  // TODO(crbug.com/40205603): Replace the SiteDataType with the Browsing Data
  // Model's StorageType, which would let us remove an enum, and let us cache
  // all accesses here.

  if (cookie_accessed_set_.count(access_details)) {
    return;
  }
  cookie_accessed_set_.insert(access_details);
  cookie_controls_->UpdateUserBypass();
}

void CookieControlsController::TabObserver::OnStatefulBounceDetected() {
  cookie_controls_->UpdateUserBypass();
}

void CookieControlsController::TabObserver::OnSubresourceBlocked() {
  cookie_controls_->OnSubresourceBlocked();
}

void CookieControlsController::TabObserver::
    OnFirstSubresourceProxiedOnCurrentPrimaryPage() const {
  cookie_controls_->OnFirstSubresourceProxiedOnCurrentPrimaryPage();
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
  cookie_controls_->UpdatePageReloadStatus(reload_count_);
  cookie_controls_->UpdateLastVisitedSitesMap();
}

void CookieControlsController::TabObserver::DidStopLoading() {
  cookie_controls_->OnPageFinishedLoading();
}

void CookieControlsController::TabObserver::ResetReloadCounter() {
  reload_count_ = 0;
}

}  // namespace content_settings
