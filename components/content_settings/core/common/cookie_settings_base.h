// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_

#include <optional>
#include <string>
#include <variant>

#include "base/containers/fixed_flat_set.h"
#include "base/types/optional_ref.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"

namespace net {
class SiteForCookies;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content_settings {

// Many CookieSettings methods handle the parameters |url|, |site_for_cookies|
// |top_frame_origin| and |first_party_url|.
//
// |url| is the URL of the requested resource.
// |site_for_cookies| is usually the URL shown in the omnibox but can also be
// empty, e.g. for subresource loads initiated from cross-site iframes, and is
// used to determine if a request is done in a third-party context.
// |top_frame_origin| is the origin shown in the omnibox.
//
// Example:
// https://a.com/index.html
// <html>
//  <body>
//    <iframe href="https://b.com/frame.html">
//      #document
//      <html>
//        <body>
//          <img href="https://a.com/img.jpg>
//          <img href="https://b.com/img.jpg>
//          <img href="https://c.com/img.jpg>
//        </body>
//      </html>
//    </iframe>
//  </body>
// </html>
//
// When each of these resources get fetched, |top_frame_origin| will always be
// "https://a.com" and |site_for_cookies| is set the following:
// https://a.com/index.html -> https://a.com/ (1p request)
// https://b.com/frame.html -> https://a.com/ (3p request)
// https://a.com/img.jpg -> <empty-url> (treated as 3p request)
// https://b.com/img.jpg -> <empty-url> (3p because from cross site iframe)
// https://c.com/img.jpg -> <empty-url> (3p request in cross site iframe)
//
// Content settings can be used to allow or block access to cookies.
// When third-party cookies are blocked, an ALLOW setting will give access to
// cookies in third-party contexts.
// The primary pattern of each setting is matched against |url|.
// The secondary pattern is matched against |top_frame_origin|.
//
// Some methods only take |url| and |first_party_url|. For |first_party_url|,
// clients either pass a value that is like |site_for_cookies| or
// |top_frame_origin|. This is done inconsistently and needs to be fixed.
class CookieSettingsBase {
 public:
  CookieSettingsBase();

  CookieSettingsBase(const CookieSettingsBase&) = delete;
  CookieSettingsBase& operator=(const CookieSettingsBase&) = delete;

  virtual ~CookieSettingsBase() = default;

  // Enum for measuring the mechanism for re-enabling third-party cookies when
  // applying 3PCD experiment. These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  //
  // Keep in sync with ThirdPartyCookieAllowMechanism at
  // //src/tools/metrics/histograms/metadata/page/enums.xml
  enum class ThirdPartyCookieAllowMechanism {
    kNone = 0,
    // Allow by explicit cookie content setting (e.g. UserBypass).
    kAllowByExplicitSetting = 1,
    // Allow by global 3p cookie setting setting (e.g. Enterprise Policy:
    // BlockThirdPartyCookies, UX).
    kAllowByGlobalSetting = 2,
    // (DEPRECATED) Allow by 3PCD metadata grants content settings. This was
    // replaced by the `kAllowBy3PCDMetadata.+` enums below.
    // kAllowBy3PCDMetadata = 3,
    // Allow by third-party cookies deprecation trial.
    kAllowBy3PCD = 4,
    kAllowBy3PCDHeuristics = 5,
    kAllowByStorageAccess = 6,
    kAllowByTopLevelStorageAccess = 7,
    // kAllowByCORSException = 8,  // Deprecated
    // Allow by 1P (AKA First Party, Top-level) DT (Deprecation Trial) token
    // being deployed.
    kAllowByTopLevel3PCD = 9,
    // Allow by Enterprise Policy (SettingSource::kPolicy):
    // CookiesAllowedForUrls.
    kAllowByEnterprisePolicyCookieAllowedForUrls = 10,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED rules.
    kAllowBy3PCDMetadataSourceUnspecified = 11,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_TEST rules.
    kAllowBy3PCDMetadataSourceTest = 12,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_1P_DT rules.
    kAllowBy3PCDMetadataSource1pDt = 13,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_3P_DT rules.
    kAllowBy3PCDMetadataSource3pDt = 14,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_DOGFOOD rules.
    kAllowBy3PCDMetadataSourceDogFood = 15,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_CRITICAL_SECTOR rules.
    kAllowBy3PCDMetadataSourceCriticalSector = 16,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_CUJ rules.
    kAllowBy3PCDMetadataSourceCuj = 17,
    // Same as kAllowBy3PCDMetadata but for
    // mojom::TpcdMetadataRuleSource::SOURCE_GOV_EDU_TLD rules.
    kAllowBy3PCDMetadataSourceGovEduTld = 18,
    // Allowed by scheme.
    kAllowByScheme = 19,
    // Allowed by tracking protection exception.
    // kAllowByTrackingProtectionException = 20,  // Deprecated
    // Allowed by sandbox 'allow-same-site-none-cookies' value.
    kAllowBySandboxValue = 21,

    kMaxValue = kAllowBySandboxValue,
  };

  // Enum for recording what type of storage permissions or overrides are
  // present on allowed cookies. These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class AllowedByStorageAccessType {
    // The cookie was not accessible through TopLevelStorageAccess or
    // StorageAccess.
    kNone = 0,

    // The cookie was accessible via TopLevelStorageAccess.
    kTopLevelOnly = 1,

    // The cookie was accessible via StorageAccess.
    kStorageAccessOnly = 2,

    // The cookie was accessible via TopLevelStorageAccess and StorageAccess
    //
    // This value should not be hit but is here for type completeness and to
    // allow metrics to accurately report the unexpected case where both
    // permissions are present, if it does occur.
    kTopLevelAndStorageAccess = 3,

    kMaxValue = kTopLevelAndStorageAccess,
  };

  // Returns true if the allow mechanism represents one of the multiple allow
  // mechanisms derived from the TPCD Mitigations Metadata.
  static bool IsAnyTpcdMetadataAllowMechanism(
      const ThirdPartyCookieAllowMechanism& mechanism);

  // Returns true if the allow mechanism corresponds to the 1P (AKA First Party,
  // Top-level) DT (Deprecation Trial).
  static bool Is1PDtRelatedAllowMechanism(
      const ThirdPartyCookieAllowMechanism& mechanism);

  static ThirdPartyCookieAllowMechanism TpcdMetadataSourceToAllowMechanism(
      const mojom::TpcdMetadataRuleSource& source);

  // MetadataSourceType exposes 3PCD metadata rule sources in UKM. It should
  // match FirstPartyMetadataSource in tools/metrics/histograms/enums.xml.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MetadataSourceType {
    None = 0,
    FirstPartyDt = 1,
    ThirdPartyDt = 2,
    CriticalSector = 3,
    CriticalSectorTld = 4,
    Cuj = 5,
    OtherMetadata = 6,
    Heuristics = 7,
  };

  static MetadataSourceType AllowMechanismToMetadataSourceType(
      const ThirdPartyCookieAllowMechanism& allow_mechanism);

  class CookieSettingWithMetadata {
   public:
    CookieSettingWithMetadata() = default;

    CookieSettingWithMetadata(
        ContentSetting cookie_setting,
        bool allow_partitioned_cookies,
        bool is_explicit_setting,
        ThirdPartyCookieAllowMechanism third_party_cookie_allow_mechanism,
        bool is_third_party_request,
        AllowedByStorageAccessType allowed_by_storage_access_type =
            AllowedByStorageAccessType::kNone);

    // Returns true iff the setting is "block" due to the user's
    // third-party-cookie-blocking setting.
    bool BlockedByThirdPartyCookieBlocking() const;

    ContentSetting cookie_setting() const { return cookie_setting_; }

    bool allow_partitioned_cookies() const {
      return allow_partitioned_cookies_;
    }

    bool is_explicit_setting() const { return is_explicit_setting_; }

    ThirdPartyCookieAllowMechanism third_party_cookie_allow_mechanism() const {
      return third_party_cookie_allow_mechanism_;
    }

    bool is_third_party_request() const { return is_third_party_request_; }

    AllowedByStorageAccessType allowed_by_storage_access_type() const {
      return allowed_by_storage_access_type_;
    }

   private:
    // The setting itself.
    ContentSetting cookie_setting_ = ContentSetting::CONTENT_SETTING_ALLOW;

    // When true, partitioned cookies will be allowed, even when the cookie
    // setting is "not allowed".
    bool allow_partitioned_cookies_ = true;

    // Whether the setting is for a specific pattern.
    bool is_explicit_setting_ = false;

    // The mechanism to enable third-party cookie access.
    ThirdPartyCookieAllowMechanism third_party_cookie_allow_mechanism_;

    // Whether the request is considered third-party.
    bool is_third_party_request_;

    // TODO( https://crbug.com/378872426): remove value when
    // `API.TopLevelStorageAccess.AllowedByStorageAccessType` is no longer being
    // collected. Evaluation of if the metric should be removed should occur no
    // earlier than 6 months after https://crbug.com/issues/379892196 is
    // completed.
    AllowedByStorageAccessType allowed_by_storage_access_type_;
  };

  // Set of types relevant for CookieSettings.
  using CookieSettingsTypeSet = base::fixed_flat_set<ContentSettingsType, 8>;

  // ContentSettings listed in this set will be automatically synced to the
  // CookieSettings instance in the network service.
  // If some types should only be synced when a certain flag is enabled, please
  // add your flag to IsContentSettingsTypeEnabled() in
  // profile_network_context_service.cc.
  static const CookieSettingsTypeSet& GetContentSettingsTypes();

  // Returns true if the cookie associated with |domain| should be deleted
  // on exit.
  // This uses domain matching as described in section 5.1.3 of RFC 6265 to
  // identify content setting rules that could have influenced the cookie
  // when it was created.
  // As |cookie_settings| can be expensive to create, it should be cached if
  // multiple calls to ShouldDeleteCookieOnExit() are made.
  //
  // This may be called on any thread.
  bool ShouldDeleteCookieOnExit(
      const ContentSettingsForOneType& cookie_settings,
      std::string_view domain,
      net::CookieSourceScheme scheme) const;

  // Returns true if the page identified by (`url`, `site_for_cookies`,
  // `top_frame_origin`, `cookie_partition_key`) is allowed to access (i.e.,
  // read or write) cookies. `site_for_cookies` is used to determine
  // third-party-ness of `url`. `top_frame_origin` is used to check if there are
  // any content_settings exceptions. `top_frame_origin` should at least be
  // specified when `site_for_cookies` is non-empty. `cookie_partition_key` is
  // used to determine if unpartitioned cookie access should be blocked based on
  // the this particular CookiePartitionKey (e.g. Fenced Frame and
  // Credentialless iframes have nonce partition keys and their access should be
  // blocked).
  //
  // This may be called on any thread.
  bool IsFullCookieAccessAllowed(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides,
      base::optional_ref<const net::CookiePartitionKey> cookie_partition_key,
      CookieSettingWithMetadata* cookie_settings = nullptr) const;

  // Returns true if the cookie set by a page identified by |url| should be
  // session only. Querying this only makes sense if |IsFullCookieAccessAllowed|
  // has returned true.
  //
  // This may be called on any thread.
  bool IsCookieSessionOnly(const GURL& url) const;

  // A helper for applying third party cookie blocking rules. Uses
  // `site_for_cookies` to determine whether the context is cross-site or not.
  ContentSetting GetCookieSetting(const GURL& url,
                                  const net::SiteForCookies& site_for_cookies,
                                  const GURL& first_party_url,
                                  net::CookieSettingOverrides overrides,
                                  SettingInfo* info = nullptr) const;

  // A helper to get third party cookie allow mechanism.
  ThirdPartyCookieAllowMechanism GetThirdPartyCookieAllowMechanism(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides,
      content_settings::SettingInfo* info = nullptr) const;

  // Returns the cookie access semantics (legacy or nonlegacy) to be applied for
  // cookies on the given domain. The |cookie_domain| can be provided as the
  // direct output of CanonicalCookie::Domain(), i.e. any leading dot does not
  // have to be removed.
  //
  // Legacy access means we treat "SameSite unspecified" as if it were
  // SameSite=None. Also, we don't require SameSite=None cookies to be Secure.
  //
  // If something is "Legacy" but explicitly says SameSite=Lax or
  // SameSite=Strict, it will still be treated as SameSite=Lax or
  // SameSite=Strict.
  //
  // Legacy behavior is based on the domain of the cookie itself, effectively
  // the domain of the requested URL, which may be embedded in another domain.
  net::CookieAccessSemantics GetCookieAccessSemanticsForDomain(
      std::string_view cookie_domain) const;

  // Gets the setting that controls whether legacy access is allowed for a given
  // cookie domain. The |cookie_domain| can be provided as the direct output of
  // CanonicalCookie::Domain(), i.e. any leading dot does not have to be
  // removed.
  ContentSetting GetSettingForLegacyCookieAccess(
      std::string_view cookie_domain) const;

  // Gets the setting that controls whether legacy scope is allowed for a given
  // cookie domain. The `cookie_domain` can be provided as the direct output of
  // CanonicalCookie::Domain(), i.e. any leading dot does not have to be
  // removed.
  ContentSetting GetSettingForLegacyCookieScope(
      std::string_view cookie_domain) const;

  // Returns the cookie legacy scope (legacy or nonlegacy) to be applied for
  // cookies on the given domain. The `cookie_domain` can be provided as the
  // direct output of CanonicalCookie::Domain(), i.e. any leading dot does not
  // have to be removed.
  //
  // Legacy behavior is based on the domain of the cookie itself, effectively
  // the domain of the requested URL, which may be embedded in another domain.
  net::CookieScopeSemantics GetCookieScopeSemanticsForDomain(
      std::string_view cookie_domain) const;

  // Returns whether a cookie should be attached regardless of its SameSite
  // value vs the request context.
  // This currently returns true if the `site_for_cookies` is a Chrome UI scheme
  // URL and the `url` is secure.
  //
  // This bypass refers to all SameSite cookies (unspecified-defaulted-into-Lax,
  // as well as explicitly specified Lax or Strict). This addresses cases where
  // the context should be treated as "first party" even if URLs have different
  // sites (or even different schemes).
  //
  // This situation can happen when chrome:// URLs embed content from https://
  // URLs. Because we trust the chrome:// scheme, and the embedded content is
  // https://, we can treat this as effectively first-party for the purposes of
  // SameSite cookies.
  //
  // This differs from "legacy SameSite behavior" because rather than the
  // requested URL, this bypass is based on the site_for_cookies, i.e. the
  // embedding context.
  virtual bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const = 0;

  // Determines whether |setting| is a valid content setting for cookies.
  static bool IsValidSetting(ContentSetting setting);
  // Determines whether |setting| means the cookie should be allowed.
  static bool IsAllowed(ContentSetting setting);

  // Determines whether |setting| is a valid content setting for legacy cookie
  // access.
  static bool IsValidSettingForLegacyAccess(ContentSetting setting);

  // Returns an indication of whether the context given by `url`,
  // `top_frame_origin`, and `site_for_cookies` has storage access,
  // given a particular set of `overrides`. Returns nullopt for same-site
  // requests.
  std::optional<net::cookie_util::StorageAccessStatus> GetStorageAccessStatus(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides,
      base::optional_ref<const net::CookiePartitionKey> cookie_partition_key,
      base::optional_ref<const network::PermissionsPolicy> permissions_policy)

      const;

 protected:
  // Returns the URL to be considered "first-party" for the given request. If
  // the `top_frame_origin` is non-empty, it is chosen; otherwise, the
  // `site_for_cookies` is used.
  static GURL GetFirstPartyURL(const net::SiteForCookies& site_for_cookies,
                               const url::Origin* top_frame_origin);

  CookieSettingWithMetadata GetCookieSettingInternal(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides,
      SettingInfo* info) const;

  // Returns true iff the query for third-party cookie access should consider
  // grants awarded by the global allowlist.
  bool ShouldConsider3pcdMetadataGrantsSettings(
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides) const;

  // The cookie behavior that may result from a cookie settings modifier
  // (`CookieSettingOverrides`).
  enum class ModifierMode {
    // Indicates that the modifiers are not enough to determine the resulting
    // cookie behavior.
    kUndefined = 0,
    // Indicates that third-party cookies are allowed due to the modifiers.
    kAllow = 1,
    // Indicates that third-party cookies are blocked but may also be unblocked
    // due to third-party cookie phaseout related mitigations (grace period,
    // heuristics, etc.)
    kPhaseout = 2,
    // Indicates that third-party cookies are blocked and cannot be unblocked
    // due to third-party cookie phaseout related mitigations (grace period,
    // heuristics, etc.)
    kBlock = 3,
  };

  // Will return the `ModifierMode` based on the `CookieSettingOverrides`
  // status.
  ModifierMode GetModifierMode(
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides) const;

  // Returns whether third-party cookies should be blocked solely due to
  // third-party-cookie "modifiers" (`CookieSettingOverrides`).
  // If the modifiers are not enough to determine a decision, `std::nullopt`
  // will be returned.
  std::optional<bool> MaybeBlockThirdPartyCookiesPerModifiers(
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides) const;

 private:
  // Returns a content setting for the requested parameters and populates |info|
  // if not null. Implementations might only implement a subset of all
  // ContentSettingsTypes. Currently only COOKIES, STORAGE_ACCESS,
  // TPCD_METADATA_GRANTS, TPCD_HEURISTICS_GRANTS, TOP_LEVEL_STORAGE_ACCESS, and
  // FEDERATED_IDENTITY_SHARING are required.
  virtual ContentSetting GetContentSetting(const GURL& primary_url,
                                           const GURL& secondary_url,
                                           ContentSettingsType content_type,
                                           SettingInfo* info) const = 0;

  struct IsAllowedWithMetadata {
    bool allowed;
    SettingInfo info;
  };

  bool IsAllowedByStorageAccessGrant(
      const GURL& url,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides) const;

  bool IsAllowedByTopLevelStorageAccessGrant(
      const GURL& url,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides) const;

  bool IsAllowedBySandboxValue(const GURL& url,
                               const GURL& first_party_url,
                               net::CookieSettingOverrides overrides) const;

  IsAllowedWithMetadata IsAllowedByTrackingProtectionSetting(
      const GURL& url,
      const GURL& first_party_url) const;

  bool IsAllowedBy3pcdHeuristicsGrantsSettings(
      const GURL& url,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides) const;

  IsAllowedWithMetadata IsAllowedBy3pcdMetadataGrantsSettings(
      const GURL& url,
      const GURL& first_party_url,
      net::CookieSettingOverrides overrides) const;

  // TODO(https://crbug.com/378872426): remove `storage_access_permissions` when
  // `API.TopLevelStorageAccess.AllowedByStorageAccessType` is no longer being
  // collected. Evaluation of if the metric should be removed should occur no
  // earlier than 6 months after https://crbug.com/issues/379892196 is
  // completed.
  struct AllowAllCookies {
    ThirdPartyCookieAllowMechanism mechanism =
        ThirdPartyCookieAllowMechanism::kNone;
    AllowedByStorageAccessType allowed_by_storage_access_type =
        AllowedByStorageAccessType::kNone;
  };
  struct AllowPartitionedCookies {};
  struct BlockAllCookies {};

  // Returns a decision on whether to allow or block the cookie request. This
  // accounts for user settings, global settings, and special cases.
  std::variant<AllowAllCookies, AllowPartitionedCookies, BlockAllCookies>
  DecideAccess(const GURL& url,
               const GURL& first_party_url,
               bool is_third_party_request,
               net::CookieSettingOverrides overrides,
               const ContentSetting& setting,
               bool is_explicit_setting,
               bool block_third_party_cookies,
               SettingInfo& setting_info) const;

  // Returns whether requests for |url| and |first_party_url| should always
  // be allowed. Called before checking other cookie settings.
  virtual bool ShouldAlwaysAllowCookies(const GURL& url,
                                        const GURL& first_party_url) const = 0;

  // Returns whether third-party cookies are blocked.
  virtual bool ShouldBlockThirdPartyCookies(
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides) const = 0;

  // Returns whether Third Party Cookie Deprecation mitigations should take
  // effect. True when mitigations are enabled for
  // 3PCD.
  bool ShouldConsiderMitigationsFor3pcd(
      net::CookieSettingOverrides overrides) const;
  // Returns whether Third Party Cookie Deprecation mitigations are enabled,
  // which requires that we are not blocking or allowing all 3PC and that either
  // 3PCD is enabled or that ForceThirdPartyCookieBlocking is enabled.
  virtual bool MitigationsEnabledFor3pcd() const = 0;

  // Returns whether |scheme| is always allowed to access 3p cookies.
  virtual bool IsThirdPartyCookiesAllowedScheme(
      std::string_view scheme) const = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
