// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/navigation_metrics/navigation_metrics.h"

#include <iterator>
#include <string>

#include "base/feature_list.h"
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
#include "url/url_canon.h"
#include "url/url_features.h"

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
    "isolated-app",
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

  // Replace non-standard separators with "." (U002E). Sometimes users may input
  // non-standard separators, causing issues when splitting labels based on ".".
  // This follows the Unicode IDNA spec:
  // https://www.unicode.org/reports/tr46/#TableDerivationStep1
  std::u16string separator_replaced_hostname;
  base::ReplaceChars(hostname16, u"\uff0e\u3002\uff61", u".",
                     &separator_replaced_hostname);

  // Keeping empty labels is necessary if there is a trailing dot, to make sure
  // `label_count` matches the `labels16` vector. See crbug.com/1362507.
  std::vector<std::u16string> labels16 =
      base::SplitString(separator_replaced_hostname, u".",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // If the canonicalized eTLD+1 has *more* labels than the full
  // noncanonicalized hostname, then there are some unexpected characters in the
  // noncanonicalized hostname (such as a user inputting %-encoded separators).
  // For simplicity (there are limits on how many edge cases it is worth
  // accounting for), just drop these cases and return early.
  if (label_count > labels16.size()) {
    return std::u16string();
  }

  size_t extra_label_count = labels16.size() - label_count;
  labels16.erase(labels16.begin(), labels16.begin() + extra_label_count);
  std::u16string noncanon_etld_plus_one = base::JoinString(labels16, u".");

  // If the extracted non-canonicalized eTLD+1 doesn't match the canonicalized
  // eTLD+1, then something is odd (e.g., mixed "." and "%2e" separators). Drop
  // these cases to avoid emitting potentially incorrect metrics.
  url::CanonHostInfo host_info;
  if (net::CanonicalizeHost(base::UTF16ToUTF8(noncanon_etld_plus_one),
                            &host_info) != etld_plus_one) {
    return std::u16string();
  }

  return noncanon_etld_plus_one;
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
    UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPage, scheme,
                              Scheme::COUNT);
    UMA_HISTOGRAM_BOOLEAN(kMainFrameHasRTLDomainDifferentPage,
                          base::i18n::StringContainsStrongRTLChars(
                              url_formatter::IDNToUnicode(url.host())));

    if (net::IsHostnameNonUnique(url.host())) {
      UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPageNonUniqueHostname,
                                scheme, Scheme::COUNT);
    }
  }

  UMA_HISTOGRAM_BOOLEAN(kMainFrameHasRTLDomain,
                        base::i18n::StringContainsStrongRTLChars(
                            url_formatter::IDNToUnicode(url.host())));

  if (is_off_the_record) {
    UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeOTR, scheme, Scheme::COUNT);
    if (!is_same_document) {
      UMA_HISTOGRAM_ENUMERATION(kMainFrameSchemeDifferentPageOTR, scheme,
                                Scheme::COUNT);
    }
  }
  UMA_HISTOGRAM_ENUMERATION(kMainFrameProfileType, profile_type);
}

void RecordOmniboxURLNavigation(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.URLNavigationScheme", GetScheme(url),
                            Scheme::COUNT);
}

IDNA2008DeviationCharacter RecordIDNA2008Metrics(
    const std::u16string& hostname16) {
  if (!url::IsRecordingIDNA2008Metrics()) {
    return IDNA2008DeviationCharacter::kNone;
  }
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
