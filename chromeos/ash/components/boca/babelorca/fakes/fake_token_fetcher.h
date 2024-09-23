// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/babelorca/token_fetcher.h"

namespace ash::babelorca {

struct TokenDataWrapper;

class FakeTokenFetcher : public TokenFetcher {
 public:
  FakeTokenFetcher();

  FakeTokenFetcher(const FakeTokenFetcher&) = delete;
  FakeTokenFetcher& operator=(const FakeTokenFetcher&) = delete;

  ~FakeTokenFetcher() override;

  // TokenFetcher:
  void FetchToken(TokenFetchCallback callback) override;

  void RespondToFetchRequest(std::optional<TokenDataWrapper> token_data);

 private:
  TokenFetchCallback fetch_callback_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_FETCHER_H_
