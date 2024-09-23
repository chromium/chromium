// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/captive_portal/core/captive_portal_types.h"

#include "base/check_op.h"

namespace captive_portal {

namespace {

const char* const kCaptivePortalResultNames[] = {
    "InternetConnected",
    "NoResponse",
    "BehindCaptivePortal",
    "NumCaptivePortalResults",
};
static_assert(std::size(kCaptivePortalResultNames) == RESULT_COUNT + 1,
              "kCaptivePortalResultNames should have "
              "RESULT_COUNT + 1 elements");

}  // namespace

std::string CaptivePortalResultToString(CaptivePortalResult result) {
  DCHECK_GE(result, 0);
  DCHECK_LT(static_cast<unsigned int>(result),
            std::size(kCaptivePortalResultNames));
  return kCaptivePortalResultNames[result];
}

}  // namespace captive_portal
