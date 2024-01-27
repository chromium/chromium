// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace page_load_metrics {

// Returns the minimum value of the optional TimeDeltas, if both values are
// set. Otherwise, if one value is set, returns that value. Otherwise, returns
// an unset value.
std::optional<base::TimeDelta> OptionalMin(
    const std::optional<base::TimeDelta>& a,
    const std::optional<base::TimeDelta>& b);

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
std::optional<std::string> GetGoogleHostnamePrefix(const GURL& url);

// Distinguishes the renderer-side timer from the browser-side timer.
enum class TimerType { kRenderer = 0, kBrowser = 1 };

// Returns the maximum amount of time to delay dispatch of metrics updates
// from the renderer process.
int GetBufferTimerDelayMillis(TimerType timer_type);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_UTIL_H_
