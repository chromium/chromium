// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace ash::babelorca {

class TokenManager {
 public:
  TokenManager(const TokenManager&) = delete;
  TokenManager& operator=(const TokenManager&) = delete;

  virtual ~TokenManager() = default;

  // Returns pointer to the token string if it has been fetched and did not
  // expire, otherwise returns null. Returned string pointer should not be
  // stored.
  virtual const std::string* GetTokenString() = 0;

  // Gets the version of the existing fetched token, 0 means no token has been
  // fetched. Useful to identify if the token has changed since the last call.
  virtual int GetFetchedVersion() = 0;

  // Fetches and stores the token, existing token will be overwritten and
  // `success_callback` will be called with `true` if fetch was successful. If
  // fetch fails, `success_callback` will be called with `false` and existing
  // token will not be overwritten. If fetch is already in progress,
  // `success_callback` will be queued and called when fetch is complete.
  virtual void ForceFetchToken(
      base::OnceCallback<void(bool)> success_callback) = 0;

 protected:
  TokenManager() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_
