// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_ACCESS_TOKEN_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAccessTokenFetcher : public AccessTokenFetcher {
 public:
  MockAccessTokenFetcher();
  ~MockAccessTokenFetcher() override;

  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)> callback) override {
    OnFetchAccessToken(callback);
  }

  MOCK_METHOD1(
      OnFetchAccessToken,
      void(base::OnceCallback<void(bool, const std::string&)>& callback));

  MOCK_METHOD1(InvalidateAccessToken, void(const std::string& access_token));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_ACCESS_TOKEN_FETCHER_H_
