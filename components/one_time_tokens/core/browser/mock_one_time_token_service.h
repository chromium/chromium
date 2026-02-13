// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_ONE_TIME_TOKEN_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_ONE_TIME_TOKEN_SERVICE_H_

#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace one_time_tokens {

class MockOneTimeTokenService : public OneTimeTokenService {
 public:
  MockOneTimeTokenService();
  ~MockOneTimeTokenService() override;
  MOCK_METHOD(void, GetRecentOneTimeTokens, (Callback callback), (override));
  MOCK_METHOD(std::vector<OneTimeToken>,
              GetCachedOneTimeTokens,
              (),
              (const override));
  MOCK_METHOD(ExpiringSubscription,
              Subscribe,
              (base::Time expiration, Callback callback),
              (override));
  MOCK_METHOD(void,
              RequestOneTimeToken,
              (base::TimeDelta timeout,
               base::OnceCallback<void(std::optional<OneTimeToken>)> callback),
              (override));
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_ONE_TIME_TOKEN_SERVICE_H_
