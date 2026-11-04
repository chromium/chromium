// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace password_manager::features {

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kRemoteActorCredentialSharing);

BASE_DECLARE_FEATURE_PARAM(std::string, kRemoteActorConfigsJson);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kRemoteActorCredentialSharingActorNameForTesting);
BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kRemoteActorCredentialSharingAllowedHostActorForTesting);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kRemoteActorCredentialSharingAllowedHostForTesting);
BASE_DECLARE_FEATURE_PARAM(std::string, kRemoteActorOAuthClientId);
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns whether remote actor credential sharing is enabled. Always returns
// false on platforms where the feature is not available.
bool RemoteActorCredentialSharingEnabled();

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_FEATURES_H_
