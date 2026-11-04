// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_

#include <array>
#include <optional>
#include <string_view>

namespace url {
class Origin;
}  // namespace url

namespace password_manager {

// Identifies the remote actor requesting credential sharing.
//
// In standard production use (when no Finch or test overrides are set), the
// actor type is determined by matching the caller's HTTPS origin against the
// built-in allowlist for each actor. When Finch (`actor_configs_json`) or test
// (`allowed_host_for_testing`) parameters are provided, those overrides take
// precedence in the following order:
// 1. `allowed_host_for_testing` (plus `allowed_host_actor_for_testing`),
// 2. `actor_configs_json` per-actor `"allowed_hosts"` lists (which augment or
//    remap the compiled-in defaults),
// 3. compiled-in default origins.
enum class RemoteActorType {
  kGemini,
  kSearch,
  kMaxValue = kSearch,
};

inline constexpr auto kAllRemoteActorTypes = std::to_array<RemoteActorType>({
    RemoteActorType::kGemini,
    RemoteActorType::kSearch,
});

// Returns the canonical string identifier (e.g. "gemini", "search") used as a
// key in `kRemoteActorConfigsJson` for `actor_type`.
std::string_view GetRemoteActorKey(RemoteActorType actor_type);

// Returns the `RemoteActorType` mapped to `origin`, or `std::nullopt` if
// remote actor credential sharing is disabled or `origin` is not allowlisted
// for any actor.
std::optional<RemoteActorType> GetRemoteActorTypeForOrigin(
    const url::Origin& origin);

// Returns true if remote actor credential sharing is allowed for `origin`.
bool IsRemoteActorCredentialSharingAllowedForOrigin(const url::Origin& origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_
