// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"

namespace safe_browsing {

TestSafeBrowsingTokenFetcher::TestSafeBrowsingTokenFetcher() = default;
TestSafeBrowsingTokenFetcher::~TestSafeBrowsingTokenFetcher() {
  // Like SafeBrowsingTokenFetchTracer, trigger the callback when destroyed.
  RunAccessTokenCallback("");
}
void TestSafeBrowsingTokenFetcher::Start(Callback callback) {
  callback_ = std::move(callback);
  was_start_called_ = true;
}
void TestSafeBrowsingTokenFetcher::RunAccessTokenCallback(std::string token) {
  if (callback_) {
    std::move(callback_).Run(token);
  }
}
bool TestSafeBrowsingTokenFetcher::WasStartCalled() {
  return was_start_called_;
}

}  // namespace safe_browsing
