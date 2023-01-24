// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_

#include <string>

#include "base/containers/enum_set.h"
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

  // This enum is provided to CookieSettingsBase query methods by callers in
  // order to indicate the reason for the query. This allows CookieSettingsBase
  // (or its subclasses) to optionally apply different policies based on how the
  // returned content setting will be used. E.g., a CookieSettings class may
  // choose to alter the value returned to the caller based on whether the
  // caller cares about the setting itself, or whether the caller just cares
  // about access to a particular cookie.
  enum class QueryReason {
    // The query is about getting the user's setting (possibly for UI exposure).
    // Storage Access API permission grants will not be considered when
    // answering the query.
    kSetting = 0,
    // Deprecated from M111. Rely directly on the individual Privacy sandbox
    // APIs in `PrivacySandboxSettings`.
    // The query is to determine whether Privacy Sandbox APIs should be enabled,
    // based on the cookies content setting. Storage Access API permission
    // grants will not be considered when answering the query.
    kPrivacySandbox,
    // The query is about access to site-scoped storage in practice, after
    // taking all settings and permission into account. Storage Access API
    // permission grants will be considered when answering the query.
    kSiteStorage,
    // The query is about determining whether cookies are accessible in
    // practice, after taking all settings and permissions into account. Storage
    // Access API permission grants will be considered when answering the query.
    kCookies,
  };

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
      net::CookieSettingOverrides overrides,
      QueryReason query_reason) const;

  // Returns true if the cookie set by a page identified by |url| should be
  // session only. Querying this only makes sense if |IsFullCookieAccessAllowed|
  // has returned true.
  //
  // This may be called on any thread.
  bool IsCookieSessionOnly(const GURL& url, QueryReason query_reason) const;

  // A helper for applying third party cookie blocking rules.
  ContentSetting GetCookieSetting(const GURL& url,
                                  const GURL& first_party_url,
                                  net::CookieSettingOverrides overrides,
                                  content_settings::SettingSource* source,
                                  QueryReason query_reason) const;

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
  virtual ContentSetting GetSettingForLegacyCookieAccess(
      const std::string& cookie_domain) const = 0;

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
  // (One such situation is e.g. chrome://print embedding some content from
  // https://accounts.google.com for Cloud Print login. Because we trust the
  // chrome:// scheme, and the embedded content is https://, we can treat this
  // as effectively first-party for the purposes of SameSite cookies.)
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

  // Returns true iff the query should consider Storage Access API permission
  // grants.
  bool ShouldConsiderStorageAccessGrants(QueryReason query_reason) const;

  // Returns true iff the query should consider top-level Storage Access API
  // permission grants. Note that this is handled similarly to storage access
  // grants, but applies to subresources more broadly (at the top-level rather
  // than only for a single frame).
  bool ShouldConsiderTopLevelStorageAccessGrants(
      QueryReason query_reason) const;

  // Static version of the above, exposed for testing.
  static bool ShouldConsiderStorageAccessGrantsInternal(
      QueryReason query_reason,
      bool storage_access_api_enabled,
      bool storage_access_api_grants_unpartitioned_storage,
      bool is_storage_partitioned);

 protected:
  // Returns true iff the request is considered third-party.
  static bool IsThirdPartyRequest(const GURL& url,
                                  const net::SiteForCookies& site_for_cookies);

  // Returns the URL to be considered "first-party" for the given request. If
  // the `top_frame_origin` is non-empty, it is chosen; otherwise, the
  // `site_for_cookies` is used.
  static GURL GetFirstPartyURL(const net::SiteForCookies& site_for_cookies,
                               const url::Origin* top_frame_origin);

 private:
  virtual ContentSetting GetCookieSettingInternal(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request,
      net::CookieSettingOverrides overrides,
      content_settings::SettingSource* source,
      QueryReason query_reason) const = 0;

  const bool storage_access_api_enabled_;
  const bool storage_access_api_grants_unpartitioned_storage_;
  const bool is_storage_partitioned_;
  const bool is_privacy_sandbox_v4_enabled_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
