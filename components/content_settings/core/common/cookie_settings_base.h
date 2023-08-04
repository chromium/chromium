// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // An enum that represents the scope of cookies to which the user's
  // third-party-cookie-blocking setting applies, in a given context.
  enum class ThirdPartyBlockingScope {
    // Access to all cookies (partitioned or unpartitioned) is blocked in this
    // context.
    kUnpartitionedAndPartitioned,
    // Access to unpartitioned cookies is blocked in this context, but access to
    // partitioned cookies is allowed.
    kUnpartitionedOnly,
  };

  class CookieSettingWithMetadata {
   public:
    CookieSettingWithMetadata(
        ContentSetting cookie_setting,
        absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope,
        bool is_explicit_setting);

    // Returns true iff the setting is "block" due to the user's
    // third-party-cookie-blocking setting.
    bool BlockedByThirdPartyCookieBlocking() const;

    bool IsPartitionedStateAllowed() const;

    ContentSetting cookie_setting() const { return cookie_setting_; }

    bool is_explicit_setting() const { return is_explicit_setting_; }

   private:
    // The setting itself.
    ContentSetting cookie_setting_;

    // The scope of cookies blocked by third-party-cookie-blocking.  The scope
    // must only be nullopt if `cookie_setting_` is not "allow", and if the
    // reason for blocking cookies is the third-party cookie blocking setting
    // (rather than a site-specific setting).
    absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope_;

    // Whether the setting is for a specific pattern.
    bool is_explicit_setting_ = false;
  };

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
      const std::string& domain,
      bool is_https) const;

  // Returns true if the page identified by (`url`, `site_for_cookies`,
  // `top_frame_origin`) is allowed to access (i.e., read or write) cookies.
  // `site_for_cookies` is used to determine third-party-ness of `url`.
  // `top_frame_origin` is used to check if there are any content_settings
  // exceptions. `top_frame_origin` should at least be specified when
  // `site_for_cookies` is non-empty.
  //
  // This may be called on any thread.
  bool IsFullCookieAccessAllowed(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      net::CookieSettingOverrides overrides) const;

  // Returns true if the cookie set by a page identified by |url| should be
  // session only. Querying this only makes sense if |IsFullCookieAccessAllowed|
  // has returned true.
  //
  // This may be called on any thread.
  bool IsCookieSessionOnly(const GURL& url) const;

  // A helper for applying third party cookie blocking rules.
  ContentSetting GetCookieSetting(const GURL& url,
                                  const GURL& first_party_url,
                                  net::CookieSettingOverrides overrides,
                                  SettingInfo* info = nullptr) const;

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
      const std::string& cookie_domain) const;

  // Gets the setting that controls whether legacy access is allowed for a given
  // cookie domain. The |cookie_domain| can be provided as the direct output of
  // CanonicalCookie::Domain(), i.e. any leading dot does not have to be
  // removed.
  ContentSetting GetSettingForLegacyCookieAccess(
      const std::string& cookie_domain) const;

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

  bool ShouldConsider3pcdSupportSettings(
      net::CookieSettingOverrides overrides) const;

  // Returns a set of overrides that includes Storage Access API and Top-Level
  // Storage Access API overrides iff the config booleans indicate that Storage
  // Access API and Top-Level Storage Access API should unlock access to DOM
  // storage.
  net::CookieSettingOverrides SettingOverridesForStorage() const;

  // Returns true iff the query should consider Storage Access API permission
  // grants.
  bool ShouldConsiderStorageAccessGrants(
      net::CookieSettingOverrides overrides) const;

  // Returns true iff the query should consider top-level Storage Access API
  // permission grants. Note that this is handled similarly to storage access
  // grants, but applies to subresources more broadly (at the top-level rather
  // than only for a single frame).
  bool ShouldConsiderTopLevelStorageAccessGrants(
      net::CookieSettingOverrides overrides) const;

  // Controls whether Storage Access API grants allow access to unpartitioned
  // *storage*, in addition to unpartitioned cookies. This is static so that all
  // instances behave consistently.
  static void SetStorageAccessAPIGrantsUnpartitionedStorageForTesting(
      bool grants);

 protected:
  // Returns true iff the request is considered third-party.
  static bool IsThirdPartyRequest(const GURL& url,
                                  const net::SiteForCookies& site_for_cookies);

  // Returns the URL to be considered "first-party" for the given request. If
  // the `top_frame_origin` is non-empty, it is chosen; otherwise, the
  // `site_for_cookies` is used.
  static GURL GetFirstPartyURL(const net::SiteForCookies& site_for_cookies,
                               const url::Origin* top_frame_origin);

  CookieSettingWithMetadata GetCookieSettingInternal(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request,
      net::CookieSettingOverrides overrides,
      SettingInfo* info) const;

 private:
  // Returns a content setting for the requested parameters and populates |info|
  // if not null. Implementations might only implement a subset of all
  // ContentSettingsTypes. Currently only COOKIES,
  // TPCD_SUPPORT, STORAGE_ACCESS and TOP_LEVEL_STORAGE_ACCESS
  // are required.
  virtual ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      SettingInfo* info = nullptr) const = 0;

  bool IsAllowedByStorageAccessGrant(const GURL& url,
                                     const GURL& first_party_url) const;

  // Returns whether requests for |url| and |first_party_url| should always
  // be allowed. Called before checking other cookie settings.
  virtual bool ShouldAlwaysAllowCookies(const GURL& url,
                                        const GURL& first_party_url) const = 0;

  // Returns whether the global 3p cookie blocking setting is enabled.
  virtual bool ShouldBlockThirdPartyCookies() const = 0;

  // Returns whether |scheme| is always allowed to access 3p cookies.
  virtual bool IsThirdPartyCookiesAllowedScheme(
      const std::string& scheme) const = 0;

  // Returns whether the StorageAccessAPI feature is enabled.
  virtual bool IsStorageAccessApiEnabled() const = 0;

  static bool storage_access_api_grants_unpartitioned_storage_;
  const bool is_storage_partitioned_;
  const bool is_privacy_sandbox_v4_enabled_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
