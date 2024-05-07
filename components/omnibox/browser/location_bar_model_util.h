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
// of the page. If |use_updated_connection_security_indicators| is true, this
// function will return the updated "connection secure" icon if |security_level|
// indicates a secure connection.
const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level,
    bool use_updated_connection_security_indicators,
    security_state::MaliciousContentStatus malicious_content_status);
}  // namespace location_bar_model

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_
