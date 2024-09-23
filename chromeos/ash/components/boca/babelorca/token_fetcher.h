// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_FETCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace ash::babelorca {

struct TokenDataWrapper;

// Interface for fetching tokens needed for authentications.
class TokenFetcher {
 public:
  // Callback executed on fetch response, no value means fetch failure.
  using TokenFetchCallback =
      base::OnceCallback<void(std::optional<TokenDataWrapper>)>;

  TokenFetcher(const TokenFetcher&) = delete;
  TokenFetcher& operator=(const TokenFetcher&) = delete;

  virtual ~TokenFetcher() = default;

  virtual void FetchToken(TokenFetchCallback callback) = 0;

 protected:
  TokenFetcher() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_FETCHER_H_
