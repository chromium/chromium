// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/features.h"

#include "base/feature_list.h"

namespace credential_management::features {

// Kill-switch for rejecting CredentialManager binding requests from PDF
// renderer processes or processes not permitted by ChildProcessSecurityPolicy
// to access data for the committed origin.
BASE_FEATURE(kCredentialManagerRejectUnauthorizedProcessOrOrigin,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace credential_management::features
