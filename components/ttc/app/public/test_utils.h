// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_
#define COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_

#include "components/ttc/app/public/tab_context_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

// Matches a `TabContextInfo` whose fields match `url` and `title`.
template <typename UrlMatcher, typename TitleMatcher>
auto TabContextIs(UrlMatcher url, TitleMatcher title) {
  return testing::AllOf(testing::Field("url", &TabContextInfo::url, url),
                        testing::Field("title", &TabContextInfo::title, title));
}

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_TEST_UTILS_H_
