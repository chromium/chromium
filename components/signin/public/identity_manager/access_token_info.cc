// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_info.h"

namespace signin {

bool operator==(const AccessTokenInfo& lhs, const AccessTokenInfo& rhs) {
  return (lhs.token == rhs.token) &&
         (lhs.expiration_time == rhs.expiration_time) &&
         (lhs.id_token == rhs.id_token);
}

}  // namespace signin
