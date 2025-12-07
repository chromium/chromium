// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/fake_spotlight_oauth_token_fetcher.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace ash::boca {

FakeSpotlightOAuthTokenFetcher::FakeSpotlightOAuthTokenFetcher(
    std::optional<std::string> oauth_token,
    std::string robot_email)
    : oauth_token_(oauth_token), robot_email_(robot_email) {}

FakeSpotlightOAuthTokenFetcher::~FakeSpotlightOAuthTokenFetcher() = default;

void FakeSpotlightOAuthTokenFetcher::Start(OAuthTokenCallback done_callback) {
  std::move(done_callback).Run(oauth_token_);
}

std::string FakeSpotlightOAuthTokenFetcher::GetDeviceRobotEmail() {
  return robot_email_;
}
}  // namespace ash::boca
