// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_RESULT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_RESULT_H_

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RemoteActorCredentialSharingResult)
enum class RemoteActorCredentialSharingResult {
  kOtherError = 0,
  kSuccess = 1,
  kUserIdentityOrSyncStateInvalid = 2,
  kNoSyncOrAccountStorage = 3,
  kNoPasswordsFound = 4,
  kUserCancelledDialog = 5,
  kAuthenticatorFailed = 6,
  kSharingServiceUnavailable = 7,
  kSharingFailed = 8,
  kRequestAlreadyInProgress = 9,
  kBlockedByPolicy = 10,
  kMaxValue = kBlockedByPolicy,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:RemoteActorCredentialSharingResult)

// Records `result` to the
// `PasswordManager.RemoteActorCredentialSharing.Result` histogram.
void LogRemoteActorCredentialSharingResult(
    RemoteActorCredentialSharingResult result);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_RESULT_H_
