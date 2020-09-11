// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_

namespace web {
class WebState;
}

namespace password_manager {

// Checks if |web_state|'s content is a secure HTML. This is done in order to
// ignore API calls from insecure context.
bool WebStateContentIsSecureHtml(const web::WebState* web_state);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
