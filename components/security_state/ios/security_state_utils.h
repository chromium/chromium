// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_IOS_SECURITY_STATE_UTILS_H_
#define COMPONENTS_SECURITY_STATE_IOS_SECURITY_STATE_UTILS_H_

#include "components/security_state/core/security_state.h"

namespace web {
class WebState;
}  // namespace web

namespace security_state {

// Returns |web_state|'s security_state::VisibleSecurityState.
std::unique_ptr<security_state::VisibleSecurityState>
GetVisibleSecurityStateForWebState(const web::WebState* web_state);

// Returns the SecurityLevel for |web_state|.
security_state::SecurityLevel GetSecurityLevelForWebState(
    const web::WebState* web_state);

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_IOS_SECURITY_STATE_UTILS_H_
