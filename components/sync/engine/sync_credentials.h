// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_

#include <string>

#include "components/signin/public/identity_manager/access_token_info.h"

namespace syncer {

// Contains everything needed to talk to and identify a user account.
struct SyncCredentials {
  // The email associated with this account.
  std::string email;

  // The OAuth2 access token info.
  signin::AccessTokenInfo access_token_info;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_
