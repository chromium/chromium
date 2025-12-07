// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include <array>
#include <iterator>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace navigation_metrics {

const char kMainFrameScheme[] = "Navigation.MainFrameScheme2";
const char kMainFrameSchemeDifferentPage[] =
    "Navigation.MainFrameSchemeDifferentPage2";
// Same as kMainFrameSchemeDifferentPage, but only recorded if the hostname is
// non-unique (e.g. http://site.test):
const char kMainFrameSchemeDifferentPageNonUniqueHostname[] =
    "Navigation.MainFrameSchemeDifferentPage2NonUniqueHostname";
const char kMainFrameSchemeOTR[] = "Navigation.MainFrameSchemeOTR2";
const char kMainFrameSchemeDifferentPageOTR[] =
    "Navigation.MainFrameSchemeDifferentPageOTR2";
const char kMainFrameHasRTLDomain[] = "Navigation.MainFrameHasRTLDomain2";
const char kMainFrameHasRTLDomainDifferentPage[] =
    "Navigation.MainFrameHasRTLDomainDifferentPage2";
const char kMainFrameProfileType[] = "Navigation.MainFrameProfileType2";
const char kMainFrameProfileTypeDifferentPage[] =
    "Navigation.MainFrameProfileTypeDifferentPage2";

namespace {

constexpr auto kSchemeNames = std::to_array<const char*>({
    "unknown",
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    url::kFtpScheme,
    url::kDataScheme,
    url::kJavaScriptScheme,
    url::kAboutScheme,
    "chrome",
    url::kBlobScheme,
    url::kFileSystemScheme,
    "chrome-native",
    "chrome-search",
    dom_distiller::kDomDistillerScheme,
    "devtools",
    "chrome-extension",
    "view-source",
    "externalfile",
    "isolated-app",
});

static_assert(std::size(kSchemeNames) == static_cast<int>(Scheme::COUNT),
              "kSchemeNames should have Scheme::COUNT elements");

}  // namespace

Scheme GetScheme(const GURL& url) {
  for (int i = static_cast<int>(Scheme::HTTP);
       i < static_cast<int>(Scheme::COUNT); ++i) {
    if (url.SchemeIs(kSchemeNames[i]))
      return static_cast<Scheme>(i);
  }
  return Scheme::UNKNOWN;
}

void RecordPrimaryMainFrameNavigation(
    const GURL& url,
    bool is_same_document,
    bool is_off_the_record,
    profile_metrics::BrowserProfileType profile_type) {
  Scheme scheme = GetScheme(url);
  UMA_HISTOGRAM_ENUMERATION(kMainFrameScheme, scheme, Scheme::COUNT);
  if (!is_same_document) {
    UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPage, scheme,
                              Scheme::COUNT);
    UMA_HISTOGRAM_BOOLEAN(kMainFrameHasRTLDomainDifferentPage,
                          base::i18n::StringContainsStrongRTLChars(
                              url_formatter::IDNToUnicode(url.GetHost())));

    if (net::IsHostnameNonUnique(url.GetHost())) {
      UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPageNonUniqueHostname,
                                scheme, Scheme::COUNT);
    }
  }

  UMA_HISTOGRAM_BOOLEAN(kMainFrameHasRTLDomain,
                        base::i18n::StringContainsStrongRTLChars(
                            url_formatter::IDNToUnicode(url.GetHost())));

  if (is_off_the_record) {
    UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeOTR, scheme, Scheme::COUNT);
    if (!is_same_document) {
      UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPageOTR, scheme,
                                Scheme::COUNT);
    }
  }
  UMA_HISTOGRAM_ENUMERATION(kMainFrameProfileType, profile_type);
  if (!is_same_document) {
    UMA_HISTOGRAM_ENUMERATION(kMainFrameProfileTypeDifferentPage, profile_type);
  }
}

void RecordOmniboxURLNavigation(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.URLNavigationScheme", GetScheme(url),
                            Scheme::COUNT);
}

}  // namespace navigation_metrics
