// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/android_native_idp_fetcher.h"

#include <memory>
#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/webid/verified_origin_resolver.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/webid/native_idp_fetcher.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chrome {

namespace {

constexpr char kTestIdpUrl[] = "https://idp.example";

}  // namespace

class AndroidNativeIdpFetcherTest : public AndroidBrowserTest {
 public:
  AndroidNativeIdpFetcherTest() = default;
  ~AndroidNativeIdpFetcherTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    content::webid::VerifiedOriginResolver::AddVerificationOverrideForTesting(
        "org.chromium.android_browsertests_apk",
        url::Origin::Create(GURL(kTestIdpUrl)));
  }
};

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveAndFetch) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL(kTestIdpUrl)));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/accounts"}));
  fetcher->Fetch(params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  // TestIdP echoes back the request and appends "Hello world!" in its reply
  // Bundle.
  ASSERT_EQ(base::StrCat({kTestIdpUrl, "/fedcm/accountsHello world!"}),
            future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest, ResolveAndFetchToken) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL(kTestIdpUrl)));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams token_params;
  token_params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/token"}));
  token_params.body = "account_123:token_request_123";
  fetcher->Fetch(token_params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  // TestIdP echoes back the request and parameters:
  ASSERT_EQ(base::StrCat({kTestIdpUrl,
                          "/fedcm/token:account_123:token_request_123Hello "
                          "world!"}),
            future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest,
                       ResolveAndFetchWithHeaders) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL(kTestIdpUrl)));
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/token"}));
  params.body = "account_789";
  params.headers["X-Custom-Header"] = "HeaderValue";
  fetcher->Fetch(params, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(base::StrCat({kTestIdpUrl,
                          "/fedcm/token:account_789:header:X-Custom-Header="
                          "HeaderValueHello world!"}),
            future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpFetcherTest,
                       ResolveFetchAccountsAndTokenSequential) {
  auto fetcher = std::make_unique<AndroidNativeIdpFetcher>(
      url::Origin::Create(GURL(kTestIdpUrl)));

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult>
      accounts_future;
  content::NativeIdpFetcher::RequestParams accounts_params;
  accounts_params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/accounts"}));
  fetcher->Fetch(accounts_params, accounts_future.GetCallback());
  ASSERT_TRUE(accounts_future.Get().has_value());
  ASSERT_EQ(base::StrCat({kTestIdpUrl, "/fedcm/accountsHello world!"}),
            accounts_future.Get().value());

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> token_future;
  content::NativeIdpFetcher::RequestParams token_params;
  token_params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/token"}));
  token_params.body = "account_456:token_request_456";
  fetcher->Fetch(token_params, token_future.GetCallback());
  ASSERT_TRUE(token_future.Get().has_value());
  ASSERT_EQ(base::StrCat({kTestIdpUrl,
                          "/fedcm/token:account_456:token_request_456Hello "
                          "world!"}),
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

class AndroidNativeIdpIntegrationTest : public AndroidBrowserTest {
 public:
  AndroidNativeIdpIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kFedCmNativeIdPs);
  }
  ~AndroidNativeIdpIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    content::webid::VerifiedOriginResolver::AddVerificationOverrideForTesting(
        "org.chromium.android_browsertests_apk",
        url::Origin::Create(GURL(kTestIdpUrl)));
  }

 protected:
  std::unique_ptr<content::NativeIdpFetcher> CreateFetcher() {
    return content::GetContentClientForTesting()
        ->browser()
        ->CreateNativeIdpFetcher(url::Origin::Create(GURL(kTestIdpUrl)));
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpIntegrationTest,
                       AccountsAndTokenEndToEndIntegration) {
  std::unique_ptr<content::NativeIdpFetcher> fetcher = CreateFetcher();
  ASSERT_NE(nullptr, fetcher);

  // 1. Fetch Accounts from the bound Android TestIdP service.
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult>
      accounts_future;
  content::NativeIdpFetcher::RequestParams accounts_params;
  accounts_params.url =
      GURL(base::StrCat({kTestIdpUrl, "/fedcm/json_accounts"}));
  fetcher->Fetch(accounts_params, accounts_future.GetCallback());
  const auto& accounts_result = accounts_future.Get();
  ASSERT_TRUE(accounts_result.has_value());

  std::optional<base::DictValue> accounts_dict =
      base::JSONReader::ReadDict(accounts_result.value(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(accounts_dict.has_value());
  const base::ListValue* accounts_list = accounts_dict->FindList("accounts");
  ASSERT_NE(nullptr, accounts_list);
  ASSERT_EQ(1u, accounts_list->size());
  const base::DictValue* first_account = (*accounts_list)[0].GetIfDict();
  ASSERT_NE(nullptr, first_account);
  const std::string* account_id = first_account->FindString("id");
  const std::string* name = first_account->FindString("name");
  const std::string* email = first_account->FindString("email");
  const std::string* given_name = first_account->FindString("given_name");
  ASSERT_NE(nullptr, account_id);
  ASSERT_NE(nullptr, name);
  ASSERT_NE(nullptr, email);
  ASSERT_NE(nullptr, given_name);
  EXPECT_EQ("1234", *account_id);
  EXPECT_EQ("Jane Doe", *name);
  EXPECT_EQ("jane@idp.example", *email);
  EXPECT_EQ("Jane", *given_name);

  // 2. Fetch Token on the same fetcher instance (reusing the native
  // connection).
  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> token_future;
  content::NativeIdpFetcher::RequestParams token_params;
  token_params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/json_token"}));
  token_params.body = "account=1234";
  fetcher->Fetch(token_params, token_future.GetCallback());
  const auto& token_result = token_future.Get();
  ASSERT_TRUE(token_result.has_value());

  std::optional<base::DictValue> token_dict =
      base::JSONReader::ReadDict(token_result.value(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(token_dict.has_value());
  const std::string* token = token_dict->FindString("token");
  ASSERT_NE(nullptr, token);
  EXPECT_EQ("sample_native_jwt_token_12345", *token);
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpIntegrationTest,
                       TokenContinueOnIntegration) {
  std::unique_ptr<content::NativeIdpFetcher> fetcher = CreateFetcher();
  ASSERT_NE(nullptr, fetcher);

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/json_continue"}));
  params.body = "account=1234";
  fetcher->Fetch(params, future.GetCallback());
  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(result.value(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());
  const std::string* continue_on = dict->FindString("continue_on");
  ASSERT_NE(nullptr, continue_on);
  EXPECT_EQ("https://idp.example/fedcm/continue", *continue_on);
}

IN_PROC_BROWSER_TEST_F(AndroidNativeIdpIntegrationTest,
                       TokenErrorResponseIntegration) {
  std::unique_ptr<content::NativeIdpFetcher> fetcher = CreateFetcher();
  ASSERT_NE(nullptr, fetcher);

  base::test::TestFuture<content::NativeIdpFetcher::FetchResult> future;
  content::NativeIdpFetcher::RequestParams params;
  params.url = GURL(base::StrCat({kTestIdpUrl, "/fedcm/json_error"}));
  params.body = "account=1234";
  fetcher->Fetch(params, future.GetCallback());
  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(result.value(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());
  const base::DictValue* error_dict = dict->FindDict("error");
  ASSERT_NE(nullptr, error_dict);
  const std::string* error_code = error_dict->FindString("code");
  ASSERT_NE(nullptr, error_code);
  EXPECT_EQ("access_denied", *error_code);
}

}  // namespace chrome
