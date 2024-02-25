// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

using AccessTokenReceivedCallback =
    base::OnceCallback<void(const std::string&)>;

// Requests the access token from `identity_manager` for the `oauth_scopes`, and
// `callback` is invoked when access token is obtained successfully or failed.
// Also records metrics of access token request status.
void RequestAccessToken(signin::IdentityManager* identity_manager,
                        const std::set<std::string>& oauth_scopes,
                        AccessTokenReceivedCallback callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_
