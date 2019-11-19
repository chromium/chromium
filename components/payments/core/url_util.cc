// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/url_util.h"

#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

// The flag for whether file:// URLs are valid.
enum class File {
  kIsAllowedForLocalDevelopment,
  kIsProhibited,
};

// The flag for whether username and password are valid in the URL.
enum class UsernamePassword {
  kNoRestrictions,
  kIsProhibited,
};

// The flag for whether only https:// scheme is allowed or other non-https
// cryptographic schemes are allowed as well.
enum class CryptographicSchemes {
  kOnlyHttps,
  kAll,
};

// The flag for whether about:blank is allowed.
enum class AboutBlank {
  kIsAllowedInPaymentHandlerWindow,
  kIsProhibited,
};

// The flag for whether path, query, and ref are allowed.
enum class PathQueryRef {
  kNoRestrictions,
  kProhibitedInSupportedOrigins,
};

// Returns whether |url| is valid for Web Payment APIs. This method
// intentionally omits default parameters so the call sites list out use case
// specific rules explicitly for easy verification against specs, for example.
bool IsValidUrlForPayments(const GURL& url,
                           File file,
                           UsernamePassword username_password,
                           CryptographicSchemes cryptographic_schemes,
                           AboutBlank about_blank,
                           PathQueryRef path_query_ref) {
  if (!url.is_valid())
    return false;

  if (url.IsAboutBlank())
    return about_blank == AboutBlank::kIsAllowedInPaymentHandlerWindow;

  if (url.SchemeIsFile())
    return file == File::kIsAllowedForLocalDevelopment;

  if (url.has_username() || url.has_password()) {
    if (username_password == UsernamePassword::kIsProhibited)
      return false;
  }

  if (url.has_ref() || url.has_query() || url.path() != "/") {
    if (path_query_ref == PathQueryRef::kProhibitedInSupportedOrigins)
      return false;
  }

  if (url.SchemeIs(url::kHttpsScheme))
    return true;

  if (url.SchemeIsCryptographic())
    return cryptographic_schemes == CryptographicSchemes::kAll;

  // Only HTTP schemes are allowed for both localhost and
  // --unsafely-treat-insecure-origin-as-secure=<origin>.
  if (!url.SchemeIs(url::kHttpScheme))
    return false;

  if (net::IsLocalhost(url))
    return true;

  // Check for --unsafely-treat-insecure-origin-as-secure=<origin>.
  return network::SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(
      url::Origin::Create(url));
}

}  // namespace

// static
bool UrlUtil::IsValidUrlBasedPaymentMethodIdentifier(const GURL& url) {
  return IsValidUrlForPayments(
      url, File::kIsProhibited, UsernamePassword::kIsProhibited,
      CryptographicSchemes::kOnlyHttps, AboutBlank::kIsProhibited,
      PathQueryRef::kNoRestrictions);
}

// static
bool UrlUtil::IsValidSupportedOrigin(const GURL& url) {
  return IsValidUrlForPayments(
      url, File::kIsProhibited, UsernamePassword::kIsProhibited,
      CryptographicSchemes::kOnlyHttps, AboutBlank::kIsProhibited,
      PathQueryRef::kProhibitedInSupportedOrigins);
}

// static
bool UrlUtil::IsValidManifestUrl(const GURL& url) {
  return IsValidUrlForPayments(
      url, File::kIsProhibited, UsernamePassword::kNoRestrictions,
      CryptographicSchemes::kAll, AboutBlank::kIsProhibited,
      PathQueryRef::kNoRestrictions);
}

// static
bool UrlUtil::IsOriginAllowedToUseWebPaymentApis(const GURL& url) {
  return IsValidUrlForPayments(
      url, File::kIsAllowedForLocalDevelopment,
      UsernamePassword::kNoRestrictions, CryptographicSchemes::kAll,
      AboutBlank::kIsProhibited, PathQueryRef::kNoRestrictions);
}

// static
bool UrlUtil::IsValidUrlInPaymentHandlerWindow(const GURL& url) {
  return IsValidUrlForPayments(url, File::kIsAllowedForLocalDevelopment,
                               UsernamePassword::kNoRestrictions,
                               CryptographicSchemes::kAll,
                               AboutBlank::kIsAllowedInPaymentHandlerWindow,
                               PathQueryRef::kNoRestrictions);
}

// static
bool UrlUtil::IsLocalDevelopmentUrl(const GURL& url) {
  DCHECK(url.is_valid());

  if (url.SchemeIsFile())
    return true;

  if (net::IsLocalhost(url))
    return true;

  return network::SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(
      url::Origin::Create(url));
}

}  // namespace payments
