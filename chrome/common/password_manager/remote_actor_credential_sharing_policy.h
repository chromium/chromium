// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_
#define CHROME_COMMON_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_

namespace url {
class Origin;
}  // namespace url

namespace password_manager {

// Returns true if remote actor credential sharing is allowed for the given
// origin.
bool IsRemoteActorCredentialSharingAllowedForOrigin(const url::Origin& origin);

}  // namespace password_manager

#endif  // CHROME_COMMON_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_POLICY_H_
