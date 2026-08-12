// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/android_native_idp_fetcher.h"

#include <memory>
#include <string>

#include "base/test/test_future.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chrome {

class AndroidNativeIdpFetcherTest : public AndroidBrowserTest {
 public:
  AndroidNativeIdpFetcherTest() = default;
  ~AndroidNativeIdpFetcherTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    content::webid::VerifiedOriginResolver::AddVerificationOverrideForTesting(
        "org.chromium.android_browsertests_apk",
        url::Origin::Create(GURL("https://idp.example")));
  }
};

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveAndFetch) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://idp.example")));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL("https://idp.example/fedcm/accounts");
  fetcher->Fetch(params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  // TestIdP echoes back the request ("https://idp.example/fedcm/accounts")
  // and appends "Hello world!" in its reply Bundle.
  ASSERT_EQ("https://idp.example/fedcm/accountsHello world!",
            future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveAndFetchToken) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://idp.example")));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams token_params;
  token_params.url = GURL("https://idp.example/fedcm/token");
  token_params.body = "account_123:token_request_123";
  fetcher->Fetch(token_params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  // TestIdP echoes back the request and parameters:
  // "https://idp.example/fedcm/token:account_123:token_request_123Hello world!"
  ASSERT_EQ(
      "https://idp.example/fedcm/token:account_123:token_request_123Hello "
      "world!",
      future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest,
                       ResolveAndFetchWithHeaders) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://idp.example")));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL("https://idp.example/fedcm/token");
  params.body = "account_789";
  params.headers["X-Custom-Header"] = "HeaderValue";
  fetcher->Fetch(params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(
      "https://idp.example/fedcm/token:account_789:header:X-Custom-Header="
      "HeaderValueHello world!",
      future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest,
                       ResolveFetchAccountsAndTokenSequential) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://idp.example")));

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult>
      accounts_future;
  content::NativeIdpFetcher::RequestParams accounts_params;
  accounts_params.url = GURL("https://idp.example/fedcm/accounts");
  fetcher->Fetch(accounts_params, accounts_future.GetCallback());
  ASSERT_TRUE(accounts_future.Get().has_value());
  ASSERT_EQ("https://idp.example/fedcm/accountsHello world!",
            accounts_future.Get().value());

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> token_future;
  content::NativeIdpFetcher::RequestParams token_params;
  token_params.url = GURL("https://idp.example/fedcm/token");
  token_params.body = "account_456:token_request_456";
  fetcher->Fetch(token_params, token_future.GetCallback());
  ASSERT_TRUE(token_future.Get().has_value());
  ASSERT_EQ(
      "https://idp.example/fedcm/token:account_456:token_request_456Hello "
      "world!",
      token_future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveUnverifiedOrigin) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://unverified-idp.example")));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL("https://unverified-idp.example/fedcm/accounts");
  fetcher->Fetch(params, future.GetCallback());
  ASSERT_FALSE(future.Get().has_value());
  ASSERT_EQ(content::NativeIdpFetcher::FetchError::kNoServiceFound,
            future.Get().error());
}

}  // namespace chrome
