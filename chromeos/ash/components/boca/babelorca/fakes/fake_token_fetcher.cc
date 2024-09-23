// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_fetcher.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "chromeos/ash/components/boca/babelorca/token_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/token_fetcher.h"

namespace ash::babelorca {

FakeTokenFetcher::FakeTokenFetcher() = default;

FakeTokenFetcher::~FakeTokenFetcher() = default;

// TokenFetcher:
void FakeTokenFetcher::FetchToken(TokenFetchCallback callback) {
  CHECK(!fetch_callback_);
  fetch_callback_ = std::move(callback);
}

void FakeTokenFetcher::RespondToFetchRequest(
    std::optional<TokenDataWrapper> token_data) {
  CHECK(fetch_callback_);
  std::move(fetch_callback_).Run(std::move(token_data));
}

}  // namespace ash::babelorca
