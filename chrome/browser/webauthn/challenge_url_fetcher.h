// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHALLENGE_URL_FETCHER_H_
#define CHROME_BROWSER_WEBAUTHN_CHALLENGE_URL_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Retrieves and caches a challenge from a challenge URL. This is owned by
// AuthenticatorRequestDialogController and exists per-request.
class ChallengeUrlFetcher {
 public:
  enum class ChallengeNotAvailableReason {
    kNotRequested,
    kWaitingForChallenge,
    kErrorFetchingChallenge,
  };

  explicit ChallengeUrlFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~ChallengeUrlFetcher();

  void FetchUrl(GURL challenge_url, base::OnceClosure callback);

  base::expected<std::vector<uint8_t>, ChallengeNotAvailableReason>
  GetChallenge();

 private:
  enum class State {
    kFetchNotStarted,
    kFetchInProgress,
    kChallengeReceived,
    kError,
  };

  void OnChallengeReceived(std::optional<std::string> response_body);

  State state_ = State::kFetchNotStarted;
  std::vector<uint8_t> challenge_;
  base::OnceClosure callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<ChallengeUrlFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHALLENGE_URL_FETCHER_H_
