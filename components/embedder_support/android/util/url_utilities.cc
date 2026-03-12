// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "components/google/core/common/google_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/util_jni/UrlUtilities_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace embedder_support {

namespace {

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    bool include_private) {
  return include_private
             ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
             : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

// Returns whether the given URLs have the same domain or host.
// See net::registry_controlled_domains::SameDomainOrHost for details.
static bool JNI_UrlUtilities_SameDomainOrHost(JNIEnv* env,
                                              const std::string& url_1_str,
                                              const std::string& url_2_str,
                                              bool include_private) {
  GURL url_1(url_1_str);
  GURL url_2(url_2_str);

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1, url_2,
                                                            filter);
}

// Returns the Domain and Registry of the given URL.
// See net::registry_controlled_domains::GetDomainAndRegistry for details.
static std::string JNI_UrlUtilities_GetDomainAndRegistry(JNIEnv* env,
                                                         const std::string& url,
                                                         bool include_private) {
  GURL gurl(url);
  if (gurl.is_empty()) {
    return std::string();
  }

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::GetDomainAndRegistry(gurl, filter);
}

// Return whether the given URL uses the Google.com domain.
// See google_util::IsGoogleDomainUrl for details.
static bool JNI_UrlUtilities_IsGoogleDomainUrl(JNIEnv* env,
                                               const std::string& url,
                                               bool allow_non_standard_port) {
  GURL gurl(url);
  if (gurl.is_empty()) {
    return false;
  }
  return google_util::IsGoogleDomainUrl(
      gurl, google_util::DISALLOW_SUBDOMAIN,
      allow_non_standard_port ? google_util::ALLOW_NON_STANDARD_PORTS
                              : google_util::DISALLOW_NON_STANDARD_PORTS);
}

// Returns whether the given URL is a Google.com domain or sub-domain.
// See google_util::IsGoogleDomainUrl for details.
static bool JNI_UrlUtilities_IsGoogleSubDomainUrl(JNIEnv* env,
                                                  const std::string& url) {
  GURL gurl(url);
  if (gurl.is_empty()) {
    return false;
  }
  return google_util::IsGoogleDomainUrl(
      gurl, google_util::ALLOW_SUBDOMAIN,
      google_util::DISALLOW_NON_STANDARD_PORTS);
}

// Returns whether the given URL is a Google.com Search URL.
// See google_util::IsGoogleSearchUrl for details.
static bool JNI_UrlUtilities_IsGoogleSearchUrl(JNIEnv* env,
                                               const std::string& url) {
  GURL gurl(url);
  if (gurl.is_empty()) {
    return false;
  }
  return google_util::IsGoogleSearchUrl(gurl);
}

// Returns whether the given URL is the Google Web Search URL.
// See google_util::IsGoogleHomePageUrl for details.
static bool JNI_UrlUtilities_IsGoogleHomePageUrl(JNIEnv* env,
                                                 const std::string& url) {
  GURL gurl(url);
  if (gurl.is_empty()) {
    return false;
  }
  return google_util::IsGoogleHomePageUrl(gurl);
}

static bool JNI_UrlUtilities_IsUrlWithinScope(JNIEnv* env,
                                              const std::string& url,
                                              const std::string& scope_url) {
  GURL gurl(url);
  GURL gscope_url(scope_url);
  return gurl.DeprecatedGetOriginAsURL() ==
             gscope_url.DeprecatedGetOriginAsURL() &&
         base::StartsWith(gurl.GetPath(), gscope_url.GetPath(),
                          base::CompareCase::SENSITIVE);
}

// Returns whether the given URLs match, ignoring the fragment portions of the
// URLs.
static bool JNI_UrlUtilities_UrlsMatchIgnoringFragments(
    JNIEnv* env,
    const std::string& url,
    const std::string& url2) {
  GURL gurl(url);
  GURL gurl2(url2);
  if (gurl.is_empty()) {
    return gurl2.is_empty();
  }
  if (!gurl.is_valid() || !gurl2.is_valid()) {
    return false;
  }

  GURL::Replacements replacements;
  replacements.SetRefStr("");
  return gurl.ReplaceComponents(replacements) ==
         gurl2.ReplaceComponents(replacements);
}

// Returns whether the given URLs have fragments that differ.
static bool JNI_UrlUtilities_UrlsFragmentsDiffer(JNIEnv* env,
                                                 const std::string& url,
                                                 const std::string& url2) {
  GURL gurl(url);
  GURL gurl2(url2);
  if (gurl.is_empty()) {
    return !gurl2.is_empty();
  }
  if (!gurl.is_valid() || !gurl2.is_valid()) {
    return true;
  }
  return gurl.GetRef() != gurl2.GetRef();
}

static std::string JNI_UrlUtilities_EscapeQueryParamValue(
    JNIEnv* env,
    const std::string& url,
    bool use_plus) {
  return base::EscapeQueryParamValue(url, use_plus);
}

static std::optional<std::string> JNI_UrlUtilities_GetValueForKeyInQuery(
    JNIEnv* env,
    const GURL& url,
    const std::string& key) {
  std::string out;
  if (!net::GetValueForKeyInQuery(url, key, &out)) {
    return std::nullopt;
  }
  return out;
}

static GURL JNI_UrlUtilities_ClearPort(JNIEnv* env, const GURL& url) {
  GURL::Replacements remove_port;
  remove_port.ClearPort();
  return url.ReplaceComponents(remove_port);
}

}  // namespace embedder_support

DEFINE_JNI(UrlUtilities)
