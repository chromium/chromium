// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_

#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace page_load_metrics {

// Returns the minimum value of the optional TimeDeltas, if both values are
// set. Otherwise, if one value is set, returns that value. Otherwise, returns
// an unset value.
base::Optional<base::TimeDelta> OptionalMin(
    const base::Optional<base::TimeDelta>& a,
    const base::Optional<base::TimeDelta>& b);

// Whether the given url has a google hostname.
bool IsGoogleHostname(const GURL& url);

// If the given hostname is a google hostname, returns the portion of the
// hostname before the google hostname. Otherwise, returns an unset optional
// value.
//
// For example:
//   https://example.com/foo => returns an unset optional value
//   https://google.com/foo => returns ''
//   https://www.google.com/foo => returns 'www'
//   https://news.google.com/foo => returns 'news'
//   https://a.b.c.google.com/foo => returns 'a.b.c'
base::Optional<std::string> GetGoogleHostnamePrefix(const GURL& url);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_
