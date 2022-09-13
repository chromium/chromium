// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include <iterator>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace navigation_metrics {

const char kMainFrameScheme[] = "Navigation.MainFrameScheme2";
const char kMainFrameSchemeDifferentPage[] =
    "Navigation.MainFrameSchemeDifferentPage2";
const char kMainFrameSchemeOTR[] = "Navigation.MainFrameSchemeOTR2";
const char kMainFrameSchemeDifferentPageOTR[] =
    "Navigation.MainFrameSchemeDifferentPageOTR2";
const char kMainFrameHasRTLDomain[] = "Navigation.MainFrameHasRTLDomain2";
const char kMainFrameHasRTLDomainDifferentPage[] =
    "Navigation.MainFrameHasRTLDomainDifferentPage2";
const char kMainFrameProfileType[] = "Navigation.MainFrameProfileType2";

namespace {

const char* const kSchemeNames[] = {
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
};

static_assert(std::size(kSchemeNames) == static_cast<int>(Scheme::COUNT),
              "kSchemeNames should have Scheme::COUNT elements");

// Returns the eTLD+1 of `hostname16`. Excludes private registries such as
// blogspot.com so that test.blogspot.com returns blogspot.com.
std::u16string GetEtldPlusOne16(const std::u16string& hostname16) {
  std::string hostname = base::UTF16ToUTF8(hostname16);
  DCHECK(!hostname.empty());
  std::string etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          hostname,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if (etld_plus_one.empty()) {
    etld_plus_one = hostname;
  }

  if (hostname == etld_plus_one) {
    return hostname16;
  }

  // etld_plus_one is normalized and doesn't contain deviation characters so
  // we can't use it for computations. Instead, manually extract the eTLD+1 from
  // hostname16 using the same number of domain labels as etld_plus_one.
  size_t label_count =
      base::ranges::count(etld_plus_one.begin(), etld_plus_one.end(), '.') + 1;

  // Keeping empty labels is necessary if there is a trailing dot, to make sure
  // `label_count` matches the `labels16` vector. See crbug.com/1362507.
  std::vector<std::u16string> labels16 = base::SplitString(
      hostname16, u".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t extra_label_count = labels16.size() - label_count;
  DCHECK_LE(extra_label_count, labels16.size());
  labels16.erase(labels16.begin(), labels16.begin() + extra_label_count);
  return base::JoinString(labels16, u".");
}

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
    UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeDifferentPage2",
                              scheme, Scheme::COUNT);
    UMA_HISTOGRAM_BOOLEAN("Navigation.MainFrameHasRTLDomainDifferentPage2",
                          base::i18n::StringContainsStrongRTLChars(
                              url_formatter::IDNToUnicode(url.host())));
  }

  UMA_HISTOGRAM_BOOLEAN("Navigation.MainFrameHasRTLDomain2",
                        base::i18n::StringContainsStrongRTLChars(
                            url_formatter::IDNToUnicode(url.host())));

  if (is_off_the_record) {
    UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeOTR2", scheme,
                              Scheme::COUNT);
    if (!is_same_document) {
      UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeDifferentPageOTR2",
                                scheme, Scheme::COUNT);
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameProfileType2", profile_type);
}

void RecordOmniboxURLNavigation(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.URLNavigationScheme", GetScheme(url),
                            Scheme::COUNT);
}

IDNA2008DeviationCharacter RecordIDNA2008Metrics(
    const std::u16string& hostname16) {
  if (hostname16.empty()) {
    return IDNA2008DeviationCharacter::kNone;
  }
  if (net::IsHostnameNonUnique(base::UTF16ToUTF8(hostname16))) {
    return IDNA2008DeviationCharacter::kNone;
  }
  std::u16string etld_plus_one = GetEtldPlusOne16(hostname16);
  if (etld_plus_one.empty()) {
    return IDNA2008DeviationCharacter::kNone;
  }
  IDNA2008DeviationCharacter c =
      url_formatter::GetDeviationCharacter(etld_plus_one);
  UMA_HISTOGRAM_BOOLEAN("Navigation.HostnameHasDeviationCharacters",
                        c != IDNA2008DeviationCharacter::kNone);
  return c;
}

}  // namespace navigation_metrics
