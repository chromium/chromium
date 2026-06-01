// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_

#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace location_bar_model {

// Get the vector icon according to security level. It indicates security state
// of the page.
const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level,
    security_state::VisibleSecurityState* visible_security_state);

// Returns the "pretty" version of the Contextual Tasks URL for display.
GURL GetContextualTasksDisplayURL(const GURL& inner_frame_url);

// Swaps the display Contextual Tasks URL identity (scheme/host/path) for its
// functional equivalent.
GURL AdjustContextualTasksURLForCopy(const GURL& url_from_text,
                                     const GURL& functional_url);

}  // namespace location_bar_model

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_UTIL_H_
