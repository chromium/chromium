// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"

#include "base/metrics/histogram_macros.h"

namespace prerender {

namespace {

const char* kOriginNames[] = {
    "[Deprecated] Link Rel Prerender (original)",
    "[Deprecated] Omnibox (original)",
    "GWS Prerender",
    "[Deprecated] Omnibox (conservative)",
    "[Deprecated] Omnibox (exact)",
    "Omnibox",
    "None",
    "Link Rel Prerender (same domain)",
    "Link Rel Prerender (cross domain)",
    "[Deprecated] Local Predictor",
    "[Deprecated] External Request",
    "[Deprecated] Instant",
    "[Deprecated] Link Rel Next",
    "[Deprecated] External Request Forced Cellular",
    "[Deprecated] Offline",
    "Navigation Predictor",
    "[Deprecated] Isolated Prerender",
    "Speculation Rules Same Origin Prerender",
    "Max",
};
static_assert(std::size(kOriginNames) == ORIGIN_MAX + 1,
              "NoStatePrefetch origin name count mismatch");

}  // namespace

const char* NameFromOrigin(Origin origin) {
  DCHECK(static_cast<int>(origin) >= 0 && origin <= ORIGIN_MAX);
  return kOriginNames[origin];
}

}  // namespace prerender
