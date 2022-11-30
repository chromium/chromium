// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CONTENT_CONTENT_UTILS_H_
#define COMPONENTS_SECURITY_STATE_CONTENT_CONTENT_UTILS_H_

#include <memory>

#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}  // namespace content

namespace security_state {

// Retrieves the visible security state from the current page in |web_contents|.
std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents);

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CONTENT_CONTENT_UTILS_H_
