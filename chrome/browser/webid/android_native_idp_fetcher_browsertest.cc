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
  fetcher->FetchAccounts(GURL("https://idp.example/fedcm/accounts"),
                         future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  // TestIdP echoes back the request ("https://idp.example/fedcm/accounts")
  // and appends "Hello world!" in its reply Bundle.
  ASSERT_EQ("https://idp.example/fedcm/accountsHello world!",
            future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveUnverifiedOrigin) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL("https://unverified-idp.example")));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  fetcher->FetchAccounts(GURL("https://unverified-idp.example/fedcm/accounts"),
                         future.GetCallback());
  ASSERT_FALSE(future.Get().has_value());
  ASSERT_EQ(content::NativeIdpFetcher::FetchError::kNoServiceFound,
            future.Get().error());
}

}  // namespace chrome
