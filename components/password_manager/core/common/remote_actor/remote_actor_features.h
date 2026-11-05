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
BASE_DECLARE_FEATURE(kRemoteActorCredentialSharingSearch);

BASE_DECLARE_FEATURE_PARAM(std::string, kRemoteActorGeminiConfigJson);
BASE_DECLARE_FEATURE_PARAM(std::string, kRemoteActorSearchConfigJson);
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns whether remote actor credential sharing is enabled for at least one
// actor (`kRemoteActorCredentialSharing` or
// `kRemoteActorCredentialSharingSearch`). Always returns false on platforms
// where the feature is not available.
bool RemoteActorCredentialSharingEnabled();

// Returns whether remote actor credential sharing is enabled for Gemini
// origins (`RemoteActorType::kGemini`). Always returns false on platforms
// where the feature is not available.
bool RemoteActorCredentialSharingGeminiEnabled();

// Returns whether remote actor credential sharing is enabled for Search Agent
// origins (`RemoteActorType::kSearch`). Always returns false on platforms
// where the feature is not available.
bool RemoteActorCredentialSharingSearchEnabled();

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_FEATURES_H_
