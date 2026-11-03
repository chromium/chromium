// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"

namespace password_manager::features {

#if !BUILDFLAG(IS_ANDROID)
// Enables sharing credentials with a remote actor (e.g. Gemini) by uploading
// them to Passbox and granting the actor permission to use them.
BASE_FEATURE(kRemoteActorCredentialSharing, base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter is for testing purposes only and must not be used in
// production. It overrides the actor name displayed in the credential
// selection dialog.
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorCredentialSharingActorNameForTesting,
                   &kRemoteActorCredentialSharing,
                   "actor_name_for_testing",
                   "Gemini");

// This parameter is for testing purposes only and must not be used in
// production. It overrides the allowlisted origins with the specified host.
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorCredentialSharingAllowedHostForTesting,
                   &kRemoteActorCredentialSharing,
                   "allowed_host_for_testing",
                   "");

// The OAuth2 client ID used when requesting remote actor permissions from APS.
BASE_FEATURE_PARAM(
    std::string,
    kRemoteActorOAuthClientId,
    &kRemoteActorCredentialSharing,
    "oauth_client_id",
    "320695880279-gnq6the97ga85scn208u5jctnk82qelk.apps.googleusercontent.com");
#endif  // !BUILDFLAG(IS_ANDROID)

bool RemoteActorCredentialSharingEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kRemoteActorCredentialSharing);
#else
  return false;
#endif
}

}  // namespace password_manager::features
