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

// Optional JSON dictionary keyed by actor identifier (e.g. "gemini", "search")
// overriding per-actor configuration and origin allowlists. Each actor entry
// may specify:
// - "allowed_hosts": JSON array of additional HTTPS origin or host strings
//   mapped to that actor (additive to compiled-in default origins, taking
//   precedence if a default origin is remapped)
// - "actor_name": UTF-8 display name shown as-is in the credential selection
//   dialog (will not be translated)
// - "learn_more_url": HTTPS URL opened from the dialog's learn-more link
// - "icon_url": HTTPS URL for the actor icon rendered in the dialog header
// - "oauth_client_id": OAuth2 client ID used for APS permission grants
// - "task_namespace_id": positive integer APS conversation namespace ID
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorConfigsJson,
                   &kRemoteActorCredentialSharing,
                   "actor_configs_json",
                   "");

// This parameter is for testing purposes only and must not be used in
// production. It overrides the actor name displayed in the credential
// selection dialog.
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorCredentialSharingActorNameForTesting,
                   &kRemoteActorCredentialSharing,
                   "actor_name_for_testing",
                   "Gemini");

// This parameter is for testing purposes only and must not be used in
// production. When `allowed_host_for_testing` is set, this selects which actor
// ("gemini" or "search") the test host maps to.
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorCredentialSharingAllowedHostActorForTesting,
                   &kRemoteActorCredentialSharing,
                   "allowed_host_actor_for_testing",
                   "");

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
