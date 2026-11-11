// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"

namespace password_manager::features {

#if !BUILDFLAG(IS_ANDROID)
// Enables sharing credentials with Gemini (`RemoteActorType::kGemini`) by
// uploading them to Passbox and granting the actor permission to use them.
BASE_FEATURE(kRemoteActorCredentialSharing,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enables sharing credentials with Search Agent (`RemoteActorType::kSearch`)
// by uploading them to Passbox and granting the actor permission to use them.
BASE_FEATURE(kRemoteActorCredentialSharingSearch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Optional JSON dictionary overriding Gemini (`RemoteActorType::kGemini`)
// configuration and origin allowlists. Supported keys:
// - "allowed_hosts": JSON array of additional HTTPS origin or host strings
//   mapped to Gemini (additive to compiled-in default origins, taking
//   precedence if a default origin is remapped)
// - "actor_name": UTF-8 display name shown as-is in the credential selection
//   dialog (will not be translated)
// - "learn_more_url": HTTPS URL opened from the dialog's learn-more link
// - "icon_url": HTTPS URL for the actor icon rendered in the dialog header
// - "oauth_client_id": OAuth2 client ID used for APS permission grants
// - "task_namespace_id": positive integer APS conversation namespace ID
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorGeminiConfigJson,
                   &kRemoteActorCredentialSharing,
                   "gemini_config_json",
                   "");

// Optional JSON dictionary overriding Search Agent (`RemoteActorType::kSearch`)
// configuration and origin allowlists. Supports the same keys as
// `kRemoteActorGeminiConfigJson`.
BASE_FEATURE_PARAM(std::string,
                   kRemoteActorSearchConfigJson,
                   &kRemoteActorCredentialSharingSearch,
                   "search_config_json",
                   "");
#endif  // !BUILDFLAG(IS_ANDROID)

bool RemoteActorCredentialSharingEnabled() {
  return RemoteActorCredentialSharingGeminiEnabled() ||
         RemoteActorCredentialSharingSearchEnabled();
}

bool RemoteActorCredentialSharingGeminiEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kRemoteActorCredentialSharing);
#else
  return false;
#endif
}

bool RemoteActorCredentialSharingSearchEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kRemoteActorCredentialSharingSearch);
#else
  return false;
#endif
}

}  // namespace password_manager::features
