// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_ACCESS_TOKEN_FETCHER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_ACCESS_TOKEN_FETCHER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// A helper class which provide access token for quick answers.
class QuickAnswersAccessTokenFetcher {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& access_token)>;

  QuickAnswersAccessTokenFetcher();

  QuickAnswersAccessTokenFetcher(const QuickAnswersAccessTokenFetcher&) =
      delete;
  QuickAnswersAccessTokenFetcher& operator=(
      const QuickAnswersAccessTokenFetcher&) = delete;

  ~QuickAnswersAccessTokenFetcher();

  void RequestAccessToken(AccessTokenCallback callback);

 private:
  void RefreshAccessToken();
  void OnAccessTokenRefreshed(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);
  void RetryRefreshAccessToken();
  void NotifyAccessTokenRefreshed();

  std::string access_token_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  // The expiration time of the |access_token_|.
  base::Time expiration_time_;

  // The buffer time to use the access token.
  base::TimeDelta token_usage_time_buffer_ = base::Minutes(1);

  base::OneShotTimer token_refresh_timer_;
  int token_refresh_error_backoff_factor_ = 1;

  std::vector<AccessTokenCallback> callbacks_;

  base::WeakPtrFactory<QuickAnswersAccessTokenFetcher> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_ACCESS_TOKEN_FETCHER_H_
