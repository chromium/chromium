// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_
#define COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_

#include "components/ttc/app/public/page_context_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

// Matches a `TabContextInfo` whose fields match `url`, `title` and `mime_type`.
template <typename UrlMatcher, typename TitleMatcher, typename MimeTypeMatcher>
auto TabContextIs(UrlMatcher url,
                  TitleMatcher title,
                  MimeTypeMatcher mime_type) {
  return testing::AllOf(
      testing::Field("url", &TabContextInfo::url, url),
      testing::Field("title", &TabContextInfo::title, title),
      testing::Field("mime_type", &TabContextInfo::mime_type, mime_type));
}

// Matches a `TabContextInfo` whose URL and title match `url` and `title`, with
// any MIME type.
template <typename UrlMatcher, typename TitleMatcher>
auto TabContextIs(UrlMatcher url, TitleMatcher title) {
  return TabContextIs(url, title, testing::_);
}

// Matches a `PageContextUpdate` holding a `SharedPageContext` whose `tab` and
// `apc` match `tab_matcher` and `apc_matcher`.
template <typename TabMatcher, typename ApcMatcher>
auto SharedContextIs(TabMatcher tab_matcher, ApcMatcher apc_matcher) {
  return testing::VariantWith<SharedPageContext>(testing::AllOf(
      testing::Field("tab", &SharedPageContext::tab, tab_matcher),
      testing::Field("apc", &SharedPageContext::apc, apc_matcher)));
}

// Matches a `PageContextUpdate` holding a `SharedPageContext` whose `tab`
// matches `tab_matcher`, with any `apc`.
template <typename TabMatcher>
auto SharedContextIs(TabMatcher tab_matcher) {
  return SharedContextIs(tab_matcher, testing::_);
}

// Matches a `PageContextUpdate` holding a `RestrictedPageContext` whose
// `reason` matches `reason_matcher`.
template <typename ReasonMatcher>
auto RestrictedContextIs(ReasonMatcher reason_matcher) {
  return testing::VariantWith<RestrictedPageContext>(
      testing::Field("reason", &RestrictedPageContext::reason, reason_matcher));
}

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_
