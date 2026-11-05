// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CONFIG_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CONFIG_H_

#include <optional>
#include <string>

#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace password_manager {

// Per-actor branding and permission parameters resolved for a remote actor.
//
// How selection works:
// 1. The calling origin is matched to a `RemoteActorType` via Finch
//    `allowed_hosts` (if provided) or compiled-in domain defaults.
// 2. Per-actor fields use Finch `gemini_config_json` or `search_config_json`
//    overrides when present and valid (non-empty strings, HTTPS URLs, positive
//    `task_namespace_id`), falling back to compiled-in defaults (GRD display
//    name, learn-more URL, OAuth client ID, and task namespace ID).
struct RemoteActorConfig {
  RemoteActorType actor_type = RemoteActorType::kGemini;
  std::u16string display_name;
  GURL learn_more_url;
  GURL icon_url;
  std::string oauth_client_id;
  int task_namespace_id = 0;
};

// Returns the resolved `RemoteActorConfig` for `actor_type`, applying any
// configured Finch feature parameter overrides.
RemoteActorConfig GetRemoteActorConfig(RemoteActorType actor_type);

// Resolves the `RemoteActorConfig` for `origin`, or `std::nullopt` if remote
// actor credential sharing is not allowed for `origin`.
std::optional<RemoteActorConfig> GetRemoteActorConfigForOrigin(
    const url::Origin& origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CONFIG_H_
