// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "components/signin/public/base/oauth_consumer_id.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

// The various results of an access token request.
//
// Keep in sync with OptimizationGuideAccessTokenResult in enums.xml.
enum class OptimizationGuideAccessTokenResult {
  kUnknown = 0,
  // The access token was received successfully.
  kSuccess = 1,
  // User was not signed-in.
  kUserNotSignedIn = 2,
  // Failed with a transient error.
  kTransientError = 3,
  // Failed with a persistent error.
  kPersistentError = 4,

  // Add new values above this line.
  kMaxValue = kPersistentError,
};

using AccessTokenReceivedCallback =
    base::OnceCallback<void(const std::string&)>;

// Handles the token request flow and invoke `callback` on completion.
// `require_token` indicates if token should be requested. When token is not
// needed `callback` is invoked immediately.
// Access token is requested from `identity_manager` for the `oauth_scopes`,
// and when access token is obtained successfully or failed, `callback` is
// invoked.
// Also records metrics of access token request status.
void HandleTokenRequestFlow(bool require_token,
                            signin::IdentityManager* identity_manager,
                            signin::OAuthConsumerId oauth_consumer_id,
                            AccessTokenReceivedCallback callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ACCESS_TOKEN_HELPER_H_
