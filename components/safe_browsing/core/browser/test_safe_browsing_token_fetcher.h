// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TEST_SAFE_BROWSING_TOKEN_FETCHER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TEST_SAFE_BROWSING_TOKEN_FETCHER_H_

#include "base/functional/callback.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class TestSafeBrowsingTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  TestSafeBrowsingTokenFetcher();
  ~TestSafeBrowsingTokenFetcher() override;

  void Start(Callback callback) override;
  void RunAccessTokenCallback(std::string token);
  bool WasStartCalled();
  MOCK_METHOD1(OnInvalidAccessToken, void(const std::string&));

 private:
  Callback callback_;
  bool was_start_called_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TEST_SAFE_BROWSING_TOKEN_FETCHER_H_
