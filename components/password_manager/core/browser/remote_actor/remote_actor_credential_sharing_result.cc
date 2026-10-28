// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"

#include "base/metrics/histogram_functions.h"

namespace password_manager {

void LogRemoteActorCredentialSharingResult(
    RemoteActorCredentialSharingResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.RemoteActorCredentialSharing.Result", result);
}

}  // namespace password_manager
