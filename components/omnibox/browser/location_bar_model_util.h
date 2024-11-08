// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_

#include "components/security_state/core/security_state.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace location_bar_model {

// Get the vector icon according to security level. It indicates security state
// of the page.
const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level,
    security_state::MaliciousContentStatus malicious_content_status);
}  // namespace location_bar_model

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_
