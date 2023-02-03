// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_MATCH_H_

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "components/history/core/browser/url_row.h"

namespace history {

// Used for intermediate history result operations.
struct HistoryMatch {
  // Required for STL, we don't use this directly.
  HistoryMatch();

  static bool EqualsGURL(const HistoryMatch& h, const GURL& url);

  // True if the url contains only a host, e.g. "http://www.google.com/".
  static bool IsHostOnly(const GURL& gurl);

  // Returns true if url in this HistoryMatch is just a host
  // (e.g. "http://www.google.com/") and not some other subpage
  // (e.g. "http://www.google.com/foo.html").
  bool IsHostOnly() const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  URLRow url_info;

  // The offset of the user's input within the URL.
  size_t input_location;

  // Whether there is a match within specific URL components. This is used
  // to prevent hiding the component containing the match. For instance,
  // if our best match was in the scheme, not showing the scheme is both
  // confusing and, for inline autocomplete of the fill_into_edit, dangerous.
  // (If the user types "h" and we match "http://foo/", we need to inline
  // autocomplete that, not "foo/", which won't show anything at all, and
  // will mislead the user into thinking the What You Typed match is what's
  // selected.)
  bool match_in_scheme;
  bool match_in_subdomain;

  // A match after any scheme/"www.", if the user input could match at both
  // locations.  If the user types "w", an innermost match ("website.com") is
  // better than a non-innermost match ("www.google.com").  If the user types
  // "x", no scheme in our prefix list (or "www.") begins with x, so all
  // matches are, vacuously, "innermost matches".
  bool innermost_match;
};

typedef base::circular_deque<HistoryMatch> HistoryMatches;

}  // namespace history

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_MATCH_H_
