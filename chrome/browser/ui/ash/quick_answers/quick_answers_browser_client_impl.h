// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_BROWSER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_BROWSER_CLIENT_IMPL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/quick_answers/controller/quick_answers_browser_client.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// A client class which provides browser access to quick answers.
class QuickAnswersBrowserClientImpl : public ash::QuickAnswersBrowserClient {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& access_token)>;

  QuickAnswersBrowserClientImpl();

  QuickAnswersBrowserClientImpl(const QuickAnswersBrowserClientImpl&) = delete;
  QuickAnswersBrowserClientImpl& operator=(
      const QuickAnswersBrowserClientImpl&) = delete;

  ~QuickAnswersBrowserClientImpl() override;

  // ash::QuickAnswersBrowserClient:
  void RequestAccessToken(AccessTokenCallback callback) override;

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
  base::TimeDelta token_usage_time_buffer_ = base::TimeDelta::FromMinutes(1);

  base::OneShotTimer token_refresh_timer_;
  int token_refresh_error_backoff_factor_ = 1;

  std::vector<AccessTokenCallback> callbacks_;

  base::WeakPtrFactory<QuickAnswersBrowserClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_BROWSER_CLIENT_IMPL_H_
