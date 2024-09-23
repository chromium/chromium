// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_ORIGIN_H_
#define COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_ORIGIN_H_

namespace prerender {

// Origin indicates what caused the NoStatePrefetch.
// NOTE: New values need to be appended, since they are used in histograms.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The values should remain
// synchronized with the enum PrerenderOrigin in
// //tools/metrics/histograms/enums.xml.
enum Origin {
  // Obsolete: ORIGIN_LINK_REL_PRERENDER = 0,
  // Obsolete: ORIGIN_OMNIBOX_ORIGINAL = 1,
  ORIGIN_GWS_PRERENDER = 2,
  // Obsolete: ORIGIN_OMNIBOX_CONSERVATIVE = 3,
  // Obsolete: ORIGIN_OMNIBOX_EXACT = 4,
  // Obsolete: ORIGIN_OMNIBOX = 5,
  ORIGIN_NONE = 6,
  ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN = 7,
  ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN = 8,
  // Obsolete: ORIGIN_LOCAL_PREDICTOR = 9,
  // Obsolete: ORIGIN_EXTERNAL_REQUEST = 10,
  // Obsolete: ORIGIN_INSTANT = 11,
  ORIGIN_LINK_REL_NEXT = 12,
  // Obsolete: ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER = 13,
  // Obsolete: ORIGIN_OFFLINE = 14,
  ORIGIN_NAVIGATION_PREDICTOR = 15,
  // Obsolete: ORIGIN_ISOLATED_PRERENDER = 16,
  ORIGIN_SAME_ORIGIN_SPECULATION = 17,
  ORIGIN_MAX = 18,
};

// Return a human-readable name for |origin|.
const char* NameFromOrigin(Origin origin);

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_ORIGIN_H_
