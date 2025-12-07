// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_FAKE_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_FAKE_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
namespace ash::boca {

class FakeSpotlightOAuthTokenFetcher : public SpotlightOAuthTokenFetcher {
 public:
  explicit FakeSpotlightOAuthTokenFetcher(
      std::optional<std::string> oauth_token,
      std::string robot_email);
  ~FakeSpotlightOAuthTokenFetcher() override;

  void Start(OAuthTokenCallback done_callback) override;

  std::string GetDeviceRobotEmail() override;

 private:
  std::optional<std::string> oauth_token_;
  std::string robot_email_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_FAKE_SPOTLIGHT_OAUTH_TOKEN_FETCHER_H_
