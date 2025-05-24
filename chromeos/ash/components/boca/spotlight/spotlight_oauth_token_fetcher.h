// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
namespace ash::boca {

class SpotlightOAuthTokenFetcher {
 public:
  using OAuthTokenCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  SpotlightOAuthTokenFetcher(const SpotlightOAuthTokenFetcher&) = delete;
  SpotlightOAuthTokenFetcher& operator=(const SpotlightOAuthTokenFetcher&) =
      delete;
  virtual ~SpotlightOAuthTokenFetcher() = default;

  virtual void Start(OAuthTokenCallback done_callback) = 0;

  virtual std::string GetDeviceRobotEmail() = 0;

 protected:
  SpotlightOAuthTokenFetcher() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_
