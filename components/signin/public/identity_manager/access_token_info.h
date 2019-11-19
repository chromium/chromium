// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

namespace signin {

// Container for a valid access token plus associated metadata.
struct AccessTokenInfo {
  // The access token itself.
  std::string token;

  // The time at which this access token will expire.
  base::Time expiration_time;

  // Contains extra information regarding the user's currently registered
  // services. It is uncommon for consumers to need to interact with this field.
  // To interact with it, first parse it via gaia::ParseServiceFlags().
  std::string id_token;

  AccessTokenInfo() = default;
  AccessTokenInfo(const std::string& token_param,
                  const base::Time& expiration_time_param,
                  const std::string& id_token)
      : token(token_param),
        expiration_time(expiration_time_param),
        id_token(id_token) {}
};

// Defined for testing purposes only.
bool operator==(const AccessTokenInfo& lhs, const AccessTokenInfo& rhs);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_INFO_H_
