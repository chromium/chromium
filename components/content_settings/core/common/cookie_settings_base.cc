// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include <functional>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

using net::cookie_util::StorageAccessResult;
using ThirdPartyCookieAllowMechanism =
    CookieSettingsBase::ThirdPartyCookieAllowMechanism;

constexpr StorageAccessResult GetStorageAccessResult(
    ThirdPartyCookieAllowMechanism mechanism) {
  using AllowMechanism = ThirdPartyCookieAllowMechanism;
  switch (mechanism) {
    case AllowMechanism::kNone:
      return StorageAccessResult::ACCESS_BLOCKED;
    case AllowMechanism::kAllowByExplicitSetting:
    case AllowMechanism::kAllowByTrackingProtectionException:
    case AllowMechanism::kAllowByGlobalSetting:
    case AllowMechanism::kAllowByEnterprisePolicyCookieAllowedForUrls:
      return StorageAccessResult::ACCESS_ALLOWED;
    case AllowMechanism::kAllowBy3PCDMetadataSource1pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case AllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case AllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
      return StorageAccessResult::ACCESS_ALLOWED_3PCD_METADATA_GRANT;
    case AllowMechanism::kAllowBy3PCD:
      return StorageAccessResult::ACCESS_ALLOWED_3PCD_TRIAL;
    case AllowMechanism::kAllowByTopLevel3PCD:
      return StorageAccessResult::ACCESS_ALLOWED_TOP_LEVEL_3PCD_TRIAL;
    case AllowMechanism::kAllowBy3PCDHeuristics:
      return StorageAccessResult::ACCESS_ALLOWED_3PCD_HEURISTICS_GRANT;
    case AllowMechanism::kAllowByStorageAccess:
      return StorageAccessResult::ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    case AllowMechanism::kAllowByTopLevelStorageAccess:
      return StorageAccessResult::ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    case AllowMechanism::kAllowByScheme:
      return StorageAccessResult::ACCESS_ALLOWED_SCHEME;
  }
}

constexpr std::optional<SettingSource> GetSettingSource(
    ThirdPartyCookieAllowMechanism mechanism) {
  using AllowMechanism = ThirdPartyCookieAllowMechanism;
  switch (mechanism) {
    // 3PCD-related mechanisms all map to `kTpcdGrant`.
    case AllowMechanism::kAllowBy3PCDMetadataSource1pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case AllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case AllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
    case AllowMechanism::kAllowBy3PCD:
    case AllowMechanism::kAllowBy3PCDHeuristics:
    case AllowMechanism::kAllowByTopLevel3PCD:
      return SettingSource::kTpcdGrant;
    // Other mechanisms do not map to a `SettingSource`.
    case AllowMechanism::kNone:
    case AllowMechanism::kAllowByExplicitSetting:
    case AllowMechanism::kAllowByTrackingProtectionException:
    case AllowMechanism::kAllowByGlobalSetting:
    case AllowMechanism::kAllowByEnterprisePolicyCookieAllowedForUrls:
    case AllowMechanism::kAllowByStorageAccess:
    case AllowMechanism::kAllowByTopLevelStorageAccess:
    case AllowMechanism::kAllowByScheme:
      return std::nullopt;
  }
}

// Returns true iff the request is considered third-party.
bool IsThirdPartyRequest(const GURL& url,
                         const net::SiteForCookies& site_for_cookies) {
  return net::StaticCookiePolicy(
             net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES)
             .CanAccessCookies(url, site_for_cookies) != net::OK;
}

}  // namespace

bool CookieSettingsBase::storage_access_api_grants_unpartitioned_storage_ =
    false;

void CookieSettingsBase::
    SetStorageAccessAPIGrantsUnpartitionedStorageForTesting(bool grants) {
  storage_access_api_grants_unpartitioned_storage_ = grants;
}

CookieSettingsBase::CookieSettingsBase()
    : is_storage_partitioned_(base::FeatureList::IsEnabled(
          net::features::kThirdPartyStoragePartitioning)) {}

CookieSettingsBase::CookieSettingWithMetadata::CookieSettingWithMetadata(
    ContentSetting cookie_setting,
    bool allow_partitioned_cookies,
    bool is_explicit_setting,
    ThirdPartyCookieAllowMechanism third_party_cookie_allow_mechanism,
    bool is_third_party_request)
    : cookie_setting_(cookie_setting),
      allow_partitioned_cookies_(allow_partitioned_cookies),
      is_explicit_setting_(is_explicit_setting),
      third_party_cookie_allow_mechanism_(third_party_cookie_allow_mechanism),
      is_third_party_request_(is_third_party_request) {}

bool CookieSettingsBase::CookieSettingWithMetadata::
    BlockedByThirdPartyCookieBlocking() const {
  const bool out = !IsAllowed(cookie_setting_) && allow_partitioned_cookies_;
  CHECK(!out || is_third_party_request_, base::NotFatalUntil::M130);
  return out;
}

// static
const CookieSettingsBase::CookieSettingsTypeSet&
CookieSettingsBase::GetContentSettingsTypes() {
  static constexpr auto kInstance =
      base::MakeFixedFlatSet<ContentSettingsType>({
          ContentSettingsType::COOKIES,
          ContentSettingsType::LEGACY_COOKIE_ACCESS,
          ContentSettingsType::STORAGE_ACCESS,
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          ContentSettingsType::TPCD_HEURISTICS_GRANTS,
          ContentSettingsType::TPCD_TRIAL,
          ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          ContentSettingsType::TRACKING_PROTECTION,
          ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
      });
  return kInstance;
}

// static
GURL CookieSettingsBase::GetFirstPartyURL(
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin) {
  return top_frame_origin != nullptr ? top_frame_origin->GetURL()
                                     : site_for_cookies.RepresentativeUrl();
}

// static
bool CookieSettingsBase::IsAnyTpcdMetadataAllowMechanism(
    const ThirdPartyCookieAllowMechanism& mechanism) {
  using AllowMechanism = ThirdPartyCookieAllowMechanism;
  switch (mechanism) {
    case AllowMechanism::kNone:
    case AllowMechanism::kAllowByExplicitSetting:
    case AllowMechanism::kAllowByTrackingProtectionException:
    case AllowMechanism::kAllowByGlobalSetting:
    case AllowMechanism::kAllowBy3PCD:
    case AllowMechanism::kAllowBy3PCDHeuristics:
    case AllowMechanism::kAllowByStorageAccess:
    case AllowMechanism::kAllowByTopLevelStorageAccess:
    case AllowMechanism::kAllowByTopLevel3PCD:
    case AllowMechanism::kAllowByEnterprisePolicyCookieAllowedForUrls:
    case AllowMechanism::kAllowByScheme:
      return false;
    case AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case AllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case AllowMechanism::kAllowBy3PCDMetadataSource1pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
      return true;
  }
}

// static
bool CookieSettingsBase::Is1PDtRelatedAllowMechanism(
    const ThirdPartyCookieAllowMechanism& mechanism) {
  using AllowMechanism = ThirdPartyCookieAllowMechanism;
  switch (mechanism) {
    case AllowMechanism::kAllowByTopLevel3PCD:
    case AllowMechanism::kAllowBy3PCDMetadataSource1pDt:
      return true;
    case AllowMechanism::kNone:
    case AllowMechanism::kAllowByExplicitSetting:
    case AllowMechanism::kAllowByTrackingProtectionException:
    case AllowMechanism::kAllowByGlobalSetting:
    case AllowMechanism::kAllowBy3PCD:
    case AllowMechanism::kAllowBy3PCDHeuristics:
    case AllowMechanism::kAllowByStorageAccess:
    case AllowMechanism::kAllowByTopLevelStorageAccess:
    case AllowMechanism::kAllowByEnterprisePolicyCookieAllowedForUrls:
    case AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case AllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case AllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
    case AllowMechanism::kAllowByScheme:
      return false;
  }
}

// static
ThirdPartyCookieAllowMechanism
CookieSettingsBase::TpcdMetadataSourceToAllowMechanism(
    const mojom::TpcdMetadataRuleSource& source) {
  using TpcdMetadataRuleSource = mojom::TpcdMetadataRuleSource;
  using AllowMechanism = ThirdPartyCookieAllowMechanism;
  switch (source) {
    case TpcdMetadataRuleSource::SOURCE_1P_DT:
      return AllowMechanism::kAllowBy3PCDMetadataSource1pDt;
    case TpcdMetadataRuleSource::SOURCE_3P_DT:
      return AllowMechanism::kAllowBy3PCDMetadataSource3pDt;
    case TpcdMetadataRuleSource::SOURCE_UNSPECIFIED:
      return AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified;
    case TpcdMetadataRuleSource::SOURCE_TEST:
      return AllowMechanism::kAllowBy3PCDMetadataSourceTest;
    case TpcdMetadataRuleSource::SOURCE_DOGFOOD:
      return AllowMechanism::kAllowBy3PCDMetadataSourceDogFood;
    case TpcdMetadataRuleSource::SOURCE_CRITICAL_SECTOR:
      return AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector;
    case TpcdMetadataRuleSource::SOURCE_CUJ:
      return AllowMechanism::kAllowBy3PCDMetadataSourceCuj;
    case TpcdMetadataRuleSource::SOURCE_GOV_EDU_TLD:
      return AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld;
  }
}

bool CookieSettingsBase::ShouldDeleteCookieOnExit(
    const ContentSettingsForOneType& cookie_settings,
    const std::string& domain,
    net::CookieSourceScheme scheme) const {
  // Cookies with an unknown (kUnset) scheme will be treated as having a not
  // secure scheme.
  GURL origin = net::cookie_util::CookieOriginToURL(
      domain, scheme == net::CookieSourceScheme::kSecure);
  // Pass GURL() as first_party_url since we don't know the context and
  // don't want to match against (*, exception) pattern.
  // No overrides are given since existing ones only pertain to 3P checks.
  ContentSetting setting =
      GetCookieSettingInternal(origin, net::SiteForCookies::FromUrl(origin),
                               GURL(), net::CookieSettingOverrides(), nullptr)
          .cookie_setting();
  DCHECK(IsValidSetting(setting));
  if (setting == CONTENT_SETTING_ALLOW) {
    return false;
  }
  // When scheme bound cookies are not enabled, non-secure cookies are readable
  // by secure sites. We need to check for the secure pattern if non-secure is
  // not allowed. The section below is independent of the scheme so we can just
  // retry from here. When scheme bound cookies are enables, this is also done
  // if the scheme is unset.
  if ((scheme == net::CookieSourceScheme::kNonSecure &&
       !net::cookie_util::IsSchemeBoundCookiesEnabled()) ||
      scheme == net::CookieSourceScheme::kUnset) {
    return ShouldDeleteCookieOnExit(cookie_settings, domain,
                                    net::CookieSourceScheme::kSecure);
  }
  // Check if there is a more precise rule that "domain matches" this cookie.
  bool matches_session_only_rule = false;
  for (const auto& entry : cookie_settings) {
    // Skip WebUI third-party cookie exceptions.
    if (entry.source == ProviderType::kWebuiAllowlistProvider &&
        !entry.secondary_pattern.MatchesAllHosts()) {
      continue;
    }
    // While we don't know on which top-frame-origin a cookie was set, we still
    // use exceptions that only specify a secondary pattern to handle cookies
    // that match this pattern.
    const std::string& host = entry.primary_pattern.MatchesAllHosts()
                                  ? entry.secondary_pattern.GetHost()
                                  : entry.primary_pattern.GetHost();
    if (net::cookie_util::IsDomainMatch(domain, host)) {
      if (entry.GetContentSetting() == CONTENT_SETTING_ALLOW) {
        return false;
      } else if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY) {
        matches_session_only_rule = true;
      }
    }
  }
  return setting == CONTENT_SETTING_SESSION_ONLY || matches_session_only_rule;
}

ContentSetting CookieSettingsBase::GetCookieSetting(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides,
    content_settings::SettingInfo* info) const {
  return GetCookieSettingInternal(url, site_for_cookies, first_party_url,
                                  overrides, info)
      .cookie_setting();
}

CookieSettingsBase::ThirdPartyCookieAllowMechanism
CookieSettingsBase::GetThirdPartyCookieAllowMechanism(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides,
    content_settings::SettingInfo* info) const {
  return GetCookieSettingInternal(url, site_for_cookies, first_party_url,
                                  overrides, info)
      .third_party_cookie_allow_mechanism();
}

bool CookieSettingsBase::IsFullCookieAccessAllowed(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    base::optional_ref<const url::Origin> top_frame_origin,
    net::CookieSettingOverrides overrides,
    CookieSettingWithMetadata* cookie_settings) const {
  CookieSettingWithMetadata setting = GetCookieSettingInternal(
      url, site_for_cookies,
      GetFirstPartyURL(site_for_cookies, top_frame_origin.as_ptr()), overrides,
      nullptr);

  if (cookie_settings) {
    *cookie_settings = setting;
  }

  return IsAllowed(setting.cookie_setting());
}

bool CookieSettingsBase::IsCookieSessionOnly(const GURL& origin) const {
  // Pass GURL() as first_party_url since we don't know the context and
  // don't want to match against (*, exception) pattern.
  // No overrides are given since existing ones only pertain to 3P checks.
  ContentSetting setting =
      GetCookieSettingInternal(origin, net::SiteForCookies::FromUrl(origin),
                               GURL(), net::CookieSettingOverrides(), nullptr)
          .cookie_setting();
  DCHECK(IsValidSetting(setting));
  return setting == CONTENT_SETTING_SESSION_ONLY;
}

net::CookieAccessSemantics
CookieSettingsBase::GetCookieAccessSemanticsForDomain(
    const std::string& cookie_domain) const {
  ContentSetting setting = GetSettingForLegacyCookieAccess(cookie_domain);
  DCHECK(IsValidSettingForLegacyAccess(setting));
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return net::CookieAccessSemantics::LEGACY;
    case CONTENT_SETTING_BLOCK:
      return net::CookieAccessSemantics::NONLEGACY;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return net::CookieAccessSemantics::UNKNOWN;
}

bool CookieSettingsBase::ShouldConsiderMitigationsFor3pcd(
    const GURL& first_party_url) const {
  // Mitigations should take effect if they are enabled (through means such as
  // 3PCD or forced 3PC phaseout) or if third-party cookies are not blocked
  // globally and the origin trial for third-party cookie deprecation is enabled
  // under `first_party_url` .
  return MitigationsEnabledFor3pcd() ||
         (!ShouldBlockThirdPartyCookies() &&
          IsBlockedByTopLevel3pcdOriginTrial(first_party_url));
}

bool CookieSettingsBase::IsBlockedByTopLevel3pcdOriginTrial(
    const GURL& first_party_url) const {
#if BUILDFLAG(IS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
             net::features::kTopLevelTpcdOriginTrial) &&
         GetContentSetting(first_party_url, first_party_url,
                           ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
                           /*info=*/nullptr) == CONTENT_SETTING_BLOCK;
#endif
}

bool CookieSettingsBase::IsAllowedBy3pcdTrialSettings(
    const GURL& url,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  return base::FeatureList::IsEnabled(net::features::kTpcdTrialSettings) &&
         !overrides.Has(net::CookieSettingOverride::kSkipTPCDTrial) &&
         ShouldConsiderMitigationsFor3pcd(first_party_url) &&
         GetContentSetting(url, first_party_url,
                           ContentSettingsType::TPCD_TRIAL,
                           /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

bool CookieSettingsBase::IsAllowedByTopLevel3pcdTrialSettings(
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  return base::FeatureList::IsEnabled(
             net::features::kTopLevelTpcdTrialSettings) &&
         !overrides.Has(net::CookieSettingOverride::kSkipTopLevelTPCDTrial) &&
         ShouldConsiderMitigationsFor3pcd(first_party_url) &&
         // Top-level 3pcd trial settings use
         // |WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE| by default and as a
         // result only use a primary pattern (with wildcard placeholder for the
         // secondary pattern).
         GetContentSetting(first_party_url, first_party_url,
                           ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                           /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

bool CookieSettingsBase::Are3pcsForceDisabledByOverride(
    net::CookieSettingOverrides overrides) const {
  return overrides.Has(
      net::CookieSettingOverride::kForceDisableThirdPartyCookies);
}

bool CookieSettingsBase::ShouldConsider3pcdMetadataGrantsSettings(
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  return base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants) &&
         !overrides.Has(net::CookieSettingOverride::kSkipTPCDMetadataGrant) &&
         ShouldConsiderMitigationsFor3pcd(first_party_url);
}

bool CookieSettingsBase::IsAllowedBy3pcdMetadataGrantsSettings(
    const GURL& url,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides,
    SettingInfo* out_info) const {
  return ShouldConsider3pcdMetadataGrantsSettings(first_party_url, overrides) &&
         IsAllowed(GetContentSetting(url, first_party_url,
                                     ContentSettingsType::TPCD_METADATA_GRANTS,
                                     out_info));
}

bool CookieSettingsBase::IsAllowedByTrackingProtectionSetting(
    const GURL& url,
    const GURL& first_party_url,
    SettingInfo& out_info) const {
  return base::FeatureList::IsEnabled(
             privacy_sandbox::kTrackingProtectionContentSettingFor3pcb) &&
         GetContentSetting(url, first_party_url,
                           ContentSettingsType::TRACKING_PROTECTION,
                           &out_info) == CONTENT_SETTING_ALLOW;
}

bool CookieSettingsBase::IsAllowedBy3pcdHeuristicsGrantsSettings(
    const GURL& url,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  return base::FeatureList::IsEnabled(
             content_settings::features::kTpcdHeuristicsGrants) &&
         features::kTpcdReadHeuristicsGrants.Get() &&
         !overrides.Has(net::CookieSettingOverride::kSkipTPCDHeuristicsGrant) &&
         ShouldConsiderMitigationsFor3pcd(first_party_url) &&
         GetContentSetting(url, first_party_url,
                           ContentSettingsType::TPCD_HEURISTICS_GRANTS,
                           /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

net::CookieSettingOverrides CookieSettingsBase::SettingOverridesForStorage()
    const {
  net::CookieSettingOverrides overrides;
  if (storage_access_api_grants_unpartitioned_storage_ ||
      is_storage_partitioned_) {
    overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
  }
  if (is_storage_partitioned_) {
    overrides.Put(
        net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
  }
  return overrides;
}

bool CookieSettingsBase::IsAllowedByTopLevelStorageAccessGrant(
    const GURL& url,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  return overrides.Has(
             net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible) &&
         GetContentSetting(url, first_party_url,
                           ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                           /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

absl::variant<CookieSettingsBase::AllowAllCookies,
              CookieSettingsBase::AllowPartitionedCookies,
              CookieSettingsBase::BlockAllCookies>
CookieSettingsBase::DecideAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    const ContentSetting& setting,
    bool is_explicit_setting,
    bool global_setting_or_embedder_blocks_third_party_cookies,
    SettingInfo& setting_info) const {
  CHECK(!url.SchemeIsWSOrWSS());

  if (!IsAllowed(setting)) {
    return BlockAllCookies{};
  }

  if (!is_third_party_request) {
    return AllowAllCookies{ThirdPartyCookieAllowMechanism::kNone};
  }

  if (!global_setting_or_embedder_blocks_third_party_cookies) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByGlobalSetting};
  }

  if (IsThirdPartyCookiesAllowedScheme(first_party_url.scheme())) {
    return AllowAllCookies{ThirdPartyCookieAllowMechanism::kAllowByScheme};
  }

  // Site controlled mechanisms (ex: web APIs, deprecation trial):
  if (IsAllowedByTopLevelStorageAccessGrant(url, first_party_url, overrides)) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByTopLevelStorageAccess};
  }
  if (IsAllowedByStorageAccessGrant(url, first_party_url, overrides)) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByStorageAccess};
  }

  if (IsAllowedBy3pcdHeuristicsGrantsSettings(url, first_party_url,
                                              overrides)) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowBy3PCDHeuristics};
  }

  // Enterprise Policies:
  if (is_explicit_setting && setting_info.source == SettingSource::kPolicy) {
    return AllowAllCookies{ThirdPartyCookieAllowMechanism::
                               kAllowByEnterprisePolicyCookieAllowedForUrls};
  }

  // Chrome controlled mechanisms (ex. 3PCD Metadata Grants):
  SettingInfo tpcd_metadata_info;
  if (IsAllowedBy3pcdMetadataGrantsSettings(url, first_party_url, overrides,
                                            &tpcd_metadata_info)) {
    return AllowAllCookies{TpcdMetadataSourceToAllowMechanism(
        tpcd_metadata_info.metadata.tpcd_metadata_rule_source())};
  }

  if (is_explicit_setting) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByExplicitSetting};
  }

  // 3PCD 1P and 3P DTs
  // New registrations are not supported for the deprecation trials, but the
  // tokens are still valid until they expire.
  // TODO(https://crbug.com/364917750): Remove this check once the trials are no
  // longer relevant.
  if (IsAllowedByTopLevel3pcdTrialSettings(first_party_url, overrides)) {
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD};
  }
  if (IsAllowedBy3pcdTrialSettings(url, first_party_url, overrides)) {
    return AllowAllCookies{ThirdPartyCookieAllowMechanism::kAllowBy3PCD};
  }

  // Check for a TRACKING_PROTECTION exception, which should also disable 3PCB.
  SettingInfo tp_info;
  if (IsAllowedByTrackingProtectionSetting(url, first_party_url, tp_info)) {
    setting_info = tp_info;
    return AllowAllCookies{
        ThirdPartyCookieAllowMechanism::kAllowByTrackingProtectionException};
  }

  return AllowPartitionedCookies{};
}

CookieSettingsBase::CookieSettingWithMetadata
CookieSettingsBase::GetCookieSettingInternal(
    const GURL& request_url,
    const net::SiteForCookies& site_for_cookies,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides,
    SettingInfo* info) const {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "ContentSettings.GetCookieSettingInternal.Duration");

  // Apply http and https exceptions to ws and wss schemes.
  std::reference_wrapper<const GURL> url = request_url;
  GURL websocket_mapped_url;
  if (url.get().SchemeIsWSOrWSS()) {
    websocket_mapped_url = net::ChangeWebSocketSchemeToHttpScheme(request_url);
    url = websocket_mapped_url;
  }

  const bool is_third_party_request =
      IsThirdPartyRequest(url, site_for_cookies);

  // Auto-allow in extensions or for WebUI embedding a secure origin.
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    if (info) {
      *info = SettingInfo{};
    }
    return CookieSettingWithMetadata{/*cookie_setting=*/CONTENT_SETTING_ALLOW,
                                     /*allow_partitioned_cookies=*/true,
                                     /*is_explicit_setting=*/false,
                                     /*third_party_cookie_allow_mechanism=*/
                                     ThirdPartyCookieAllowMechanism::kNone,
                                     is_third_party_request};
  }

  SettingInfo setting_info;
  ContentSetting cookie_setting = GetContentSetting(
      url, first_party_url, ContentSettingsType::COOKIES, &setting_info);

  bool is_explicit_setting = !setting_info.primary_pattern.MatchesAllHosts() ||
                             !setting_info.secondary_pattern.MatchesAllHosts();

  // `ShouldBlockThirdPartyCookies()` is true iff the 3PC are blocked globally
  // (either by the user, or by 3PCD). `Are3pcsForceDisabledByOverride()` is
  // true iff the embedder forcibly blocks 3PCs.
  //
  // This variable is a function of 3PC policy, but is not the final say. Some
  // exemptions can allow 3PCs in this context, even when this variable is true.
  const bool global_setting_or_embedder_blocks_third_party_cookies =
      ShouldBlockThirdPartyCookies() ||
      Are3pcsForceDisabledByOverride(overrides) ||
      IsBlockedByTopLevel3pcdOriginTrial(first_party_url);

  const absl::variant<AllowAllCookies, AllowPartitionedCookies, BlockAllCookies>
      choice = DecideAccess(
          url, first_party_url, is_third_party_request, overrides,
          cookie_setting, is_explicit_setting,
          global_setting_or_embedder_blocks_third_party_cookies, setting_info);

  if (const AllowAllCookies* allow_cookies =
          absl::get_if<AllowAllCookies>(&choice)) {
    CHECK(IsAllowed(cookie_setting));
    CHECK(!is_third_party_request ||
              !global_setting_or_embedder_blocks_third_party_cookies ||
              allow_cookies->mechanism != ThirdPartyCookieAllowMechanism::kNone,
          base::NotFatalUntil::M128);
    // `!is_third_party_request` implies that the exemption reason must be
    // kNone. (It doesn't make sense to exempt a first-party cookie from 3PCD.)
    CHECK(is_third_party_request ||
              allow_cookies->mechanism == ThirdPartyCookieAllowMechanism::kNone,
          base::NotFatalUntil::M128);

    FireStorageAccessHistogram(
        GetStorageAccessResult(allow_cookies->mechanism));

    if (allow_cookies->mechanism ==
        ThirdPartyCookieAllowMechanism::kAllowByTrackingProtectionException) {
      is_explicit_setting = true;
      cookie_setting = CONTENT_SETTING_ALLOW;
    }
    if (info) {
      if (std::optional<SettingSource> source =
              GetSettingSource(allow_cookies->mechanism);
          source.has_value()) {
        setting_info.source = *source;
      }
      *info = setting_info;
    }
    const CookieSettingWithMetadata out{
        cookie_setting,
        /*allow_partitioned_cookies=*/true,
        is_explicit_setting,
        /*third_party_cookie_allow_mechanism=*/allow_cookies->mechanism,
        is_third_party_request,
    };
    CHECK(!out.BlockedByThirdPartyCookieBlocking());
    CHECK(out.allow_partitioned_cookies());
    return out;
  }

  if (absl::holds_alternative<AllowPartitionedCookies>(choice)) {
    CHECK(is_third_party_request, base::NotFatalUntil::M128);
    CHECK(global_setting_or_embedder_blocks_third_party_cookies,
          base::NotFatalUntil::M128);
    CHECK(!is_explicit_setting, base::NotFatalUntil::M128);

    FireStorageAccessHistogram(StorageAccessResult::ACCESS_BLOCKED);

    if (info) {
      *info = setting_info;
    }
    const CookieSettingWithMetadata out{
        CONTENT_SETTING_BLOCK,
        /*allow_partitioned_cookies=*/true,
        is_explicit_setting,
        ThirdPartyCookieAllowMechanism::kNone,
        is_third_party_request,
    };
    CHECK(out.BlockedByThirdPartyCookieBlocking());
    CHECK(out.allow_partitioned_cookies());
    return out;
  }

  CHECK(absl::holds_alternative<BlockAllCookies>(choice));
  CHECK_EQ(cookie_setting, CONTENT_SETTING_BLOCK, base::NotFatalUntil::M128);
  FireStorageAccessHistogram(StorageAccessResult::ACCESS_BLOCKED);

  if (info) {
    *info = setting_info;
  }
  const CookieSettingWithMetadata out{
      CONTENT_SETTING_BLOCK,
      /*allow_partitioned_cookies=*/false,
      is_explicit_setting,
      ThirdPartyCookieAllowMechanism::kNone,
      is_third_party_request,
  };
  CHECK(!out.BlockedByThirdPartyCookieBlocking());
  CHECK(!out.allow_partitioned_cookies());
  return out;
}

std::optional<net::cookie_util::StorageAccessStatus>
CookieSettingsBase::GetStorageAccessStatus(
    const GURL& url,
    const net::SiteForCookies& site_for_for_cookies,
    base::optional_ref<const url::Origin> top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  if (!IsThirdPartyRequest(url, site_for_for_cookies)) {
    return std::nullopt;
  }
  if (IsFullCookieAccessAllowed(url, site_for_for_cookies, top_frame_origin,
                                overrides)) {
    return net::cookie_util::StorageAccessStatus::kActive;
  }
  overrides.Put(
      net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);
  if (IsFullCookieAccessAllowed(url, site_for_for_cookies, top_frame_origin,
                                overrides)) {
    return net::cookie_util::StorageAccessStatus::kInactive;
  }
  return net::cookie_util::StorageAccessStatus::kNone;
}

bool CookieSettingsBase::IsAllowedByStorageAccessGrant(
    const GURL& url,
    const GURL& first_party_url,
    net::CookieSettingOverrides overrides) const {
  if (!overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible) &&
      !overrides.Has(
          net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader)) {
    return false;
  }
  // The Storage Access API allows access in A(B(A)) case (or similar). Do the
  // same-origin check first for performance reasons.
  const url::Origin origin = url::Origin::Create(url);
  const url::Origin first_party_origin = url::Origin::Create(first_party_url);
  if (origin.IsSameOriginWith(first_party_origin) ||
      net::SchemefulSite(origin) == net::SchemefulSite(first_party_origin)) {
    return true;
  }
  if (GetContentSetting(url, first_party_url,
                        ContentSettingsType::STORAGE_ACCESS,
                        /*info=*/nullptr) == CONTENT_SETTING_ALLOW) {
    return true;
  }
  // Note: If the `kStorageAccessGrantEligible` override is present, then the
  // browser process must have already verified the presence of the permissions
  // policy for this access. If the appropriate permissions policy was not
  // present, then no matching FEDERATED_IDENTITY_SHARING setting would be sent
  // to this instance from the browser process.
  return overrides.Has(
             net::CookieSettingOverride::kStorageAccessGrantEligible) &&
         GetContentSetting(url, first_party_url,
                           ContentSettingsType::FEDERATED_IDENTITY_SHARING,
                           /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

ContentSetting CookieSettingsBase::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain) const {
  // The content setting patterns are treated as domains, not URLs, so the
  // scheme is irrelevant (so we can just arbitrarily pass false).
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie_domain, false /* secure scheme */);

  return GetContentSetting(cookie_domain_url, GURL(),
                           ContentSettingsType::LEGACY_COOKIE_ACCESS,
                           /*info=*/nullptr);
}

// static
bool CookieSettingsBase::IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

// static
bool CookieSettingsBase::IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

// static
bool CookieSettingsBase::IsValidSettingForLegacyAccess(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW || setting == CONTENT_SETTING_BLOCK);
}

}  // namespace content_settings
