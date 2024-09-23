// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/common/url_loader_throttle.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace url_rewrite {
namespace {

constexpr char kMixedCaseCorsExemptHeader[] = "CoRs-ExEmPt";
constexpr char kUpperCaseCorsExemptHeader[] = "CORS-EXEMPT";
constexpr char kMixedCaseCorsExemptHeader2[] = "Another-CoRs-ExEmPt-2";
constexpr char kUpperCaseCorsExemptHeader2[] = "ANOTHER-CORS-EXEMPT-2";
constexpr char kRequiresCorsHeader[] = "requires-cors";

}  // namespace

class URLLoaderThrottleTest : public testing::Test {
 public:
  URLLoaderThrottleTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~URLLoaderThrottleTest() override = default;

  URLLoaderThrottle::IsHeaderCorsExemptCallback CreateCorsExemptHeadersCallback(
      std::vector<std::string> cors_exempt_headers) {
    return base::BindLambdaForTesting(
        [cors_exempt_headers](std::string_view header) {
          for (const auto& exempt_header : cors_exempt_headers) {
            if (base::EqualsCaseInsensitiveASCII(header, exempt_header)) {
              return true;
            }
          }
          return false;
        });
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests rules are properly applied when wildcard-filtering is used on hosts.
TEST_F(URLLoaderThrottleTest, WildcardHosts) {
  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.push_back(mojom::UrlHeader::New("Header", "Value"));
  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = std::optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);
  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  network::ResourceRequest request1;
  request1.url = GURL("http://test.net");
  throttle.WillStartRequest(&request1, &defer);
  EXPECT_TRUE(request1.headers.HasHeader("Header"));

  network::ResourceRequest request2;
  request2.url = GURL("http://subdomain.test.net");
  throttle.WillStartRequest(&request2, &defer);
  EXPECT_TRUE(request2.headers.HasHeader("Header"));

  network::ResourceRequest request3;
  request3.url = GURL("http://domaintest.net");
  throttle.WillStartRequest(&request3, &defer);
  EXPECT_FALSE(request3.headers.HasHeader("Header"));

  network::ResourceRequest request4;
  request4.url = GURL("http://otherdomain.net");
  throttle.WillStartRequest(&request4, &defer);
  EXPECT_FALSE(request4.headers.HasHeader("Header"));
}

// Verifies that injected headers are correctly exempted from CORS checks if
// their names are registered as CORS exempt.
TEST_F(URLLoaderThrottleTest, CorsAwareHeaders) {
  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.push_back(
      mojom::UrlHeader::New(kRequiresCorsHeader, "Value"));

  // Inject the uppercased form for CORS exempt header #1, and the mixed case
  // form of header #2.
  add_headers->headers.push_back(
      mojom::UrlHeader::New(kUpperCaseCorsExemptHeader, "Value"));
  add_headers->headers.push_back(
      mojom::UrlHeader::New(kMixedCaseCorsExemptHeader2, "Value"));

  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = std::optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  // Use the mixed case form for CORS exempt header #1, and the uppercased form
  // of header #2.
  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback(
          {kMixedCaseCorsExemptHeader, kUpperCaseCorsExemptHeader2}));

  network::ResourceRequest request;
  request.url = GURL("http://test.net");
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  // Verify that the cors-exempt and cors-required headers were partitioned into
  // the "cors_exempt_headers" and "headers" arrays, respectively.
  EXPECT_TRUE(
      request.cors_exempt_headers.HasHeader(kUpperCaseCorsExemptHeader));
  EXPECT_TRUE(
      request.cors_exempt_headers.HasHeader(kMixedCaseCorsExemptHeader2));
  EXPECT_TRUE(request.headers.HasHeader(kRequiresCorsHeader));

  // Verify that the headers were not also placed in the other array.
  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(kRequiresCorsHeader));
  EXPECT_FALSE(request.headers.HasHeader(kUpperCaseCorsExemptHeader));
  EXPECT_FALSE(request.headers.HasHeader(kMixedCaseCorsExemptHeader2));
}

// Tests URL replacement rules that replace to a data URL do not append query or
// ref from the original URL.
TEST_F(URLLoaderThrottleTest, DataReplacementUrl) {
  constexpr char kCssDataURI[] = "data:text/css,";

  mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
      mojom::UrlRequestRewriteReplaceUrl::New();
  replace_url->url_ends_with = ".css";
  replace_url->new_url = GURL(kCssDataURI);
  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewReplaceUrl(std::move(replace_url));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = std::optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  network::ResourceRequest request;
  request.url = GURL("http://test.net/style.css?query#ref");
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, std::string_view(kCssDataURI));
}

// Tests URL replacement rules do not apply more than once in a redirect chain
// on the same host.
TEST_F(URLLoaderThrottleTest, RedirectsToSameHost) {
  constexpr char kAppendQueryString[] = "foo=1&bar=2";
  constexpr char kBaseUrl[] = "http://test.net";
  constexpr char kUrlWithQueryString[] = "http://test.net?foo=1&bar=2";

  mojom::UrlRequestRewriteAppendToQueryPtr append_query =
      mojom::UrlRequestRewriteAppendToQuery::New(kAppendQueryString);
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(
      mojom::UrlRequestAction::NewAppendToQuery(std::move(append_query)));

  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = std::optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  network::ResourceRequest request;
  request.url = GURL(kBaseUrl);
  request.navigation_redirect_chain = {GURL(kBaseUrl)};
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kUrlWithQueryString));

  request.url = GURL(kUrlWithQueryString);
  request.navigation_redirect_chain = {GURL(kBaseUrl),
                                       GURL(kUrlWithQueryString)};
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kUrlWithQueryString));
}

// Tests URL replacement rules do not apply when redirecting.
TEST_F(URLLoaderThrottleTest, RedirectsFromDifferentHost) {
  constexpr char kAppendQueryString[] = "foo=1&bar=2";
  constexpr char kBaseUrl1[] = "http://a.com";
  constexpr char kBaseUrl2[] = "http://b.com";

  mojom::UrlRequestRewriteAppendToQueryPtr append_query =
      mojom::UrlRequestRewriteAppendToQuery::New(kAppendQueryString);
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(
      mojom::UrlRequestAction::NewAppendToQuery(std::move(append_query)));

  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = std::optional<std::vector<std::string>>({"*.b.com"});
  rule->actions = std::move(actions);

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  network::ResourceRequest request;
  request.url = GURL(kBaseUrl1);
  request.navigation_redirect_chain = {GURL(kBaseUrl1)};
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kBaseUrl1));

  request.url = GURL(kBaseUrl2);
  request.navigation_redirect_chain = {GURL(kBaseUrl1), GURL(kBaseUrl2)};
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kBaseUrl2));
}

// Tests URL replacement rules do not apply more than once when redirecting to a
// different host that the rule applies to.
TEST_F(URLLoaderThrottleTest, RedirectsToDifferentHost) {
  constexpr char kAppendQueryString[] = "foo=1&bar=2";
  constexpr char kBaseUrl1[] = "http://a.com";
  constexpr char kBaseUrl2[] = "http://b.com";
  constexpr char kUrl1WithQueryString[] = "http://a.com?foo=1&bar=2";

  mojom::UrlRequestRewriteAppendToQueryPtr append_query =
      mojom::UrlRequestRewriteAppendToQuery::New(kAppendQueryString);
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(
      mojom::UrlRequestAction::NewAppendToQuery(std::move(append_query)));

  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter =
      std::optional<std::vector<std::string>>({"*.a.com", "*.b.com"});
  rule->actions = std::move(actions);

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  network::ResourceRequest request;
  request.url = GURL(kBaseUrl1);
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kUrl1WithQueryString));

  request.url = GURL(kBaseUrl2);
  request.navigation_redirect_chain = {GURL(kBaseUrl1), GURL(kBaseUrl2)};
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, GURL(kBaseUrl2));
}

class TestThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  TestThrottleDelegate() = default;
  ~TestThrottleDelegate() override = default;

  bool canceled() const { return canceled_; }
  std::string_view cancel_reason() const { return cancel_reason_; }

  void Reset() {
    canceled_ = false;
    cancel_reason_.clear();
  }

  // URLLoaderThrottle::Delegate implementation.
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    canceled_ = true;
    cancel_reason_ = std::string(custom_reason);
  }
  void Resume() override {}

 private:
  bool canceled_ = false;
  std::string cancel_reason_;
};

// Tests that resource loads can be allowed or blocked based on the
// UrlRequestAction policy.
TEST_F(URLLoaderThrottleTest, AllowAndDeny) {
  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();

  {
    mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
    rule->hosts_filter = std::optional<std::vector<std::string>>({"test.net"});
    rule->actions.push_back(mojom::UrlRequestAction::NewPolicy(
        mojom::UrlRequestAccessPolicy::kAllow));
    rules->rules.push_back(std::move(rule));
  }
  {
    mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
    rule->actions.push_back(mojom::UrlRequestAction::NewPolicy(
        mojom::UrlRequestAccessPolicy::kDeny));
    rules->rules.push_back(std::move(rule));
  }

  URLLoaderThrottle throttle(
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules)),
      CreateCorsExemptHeadersCallback({}));
  bool defer = false;

  TestThrottleDelegate delegate;
  throttle.set_delegate(&delegate);

  network::ResourceRequest request1;
  request1.url = GURL("http://test.net");
  throttle.WillStartRequest(&request1, &defer);
  EXPECT_FALSE(delegate.canceled());

  delegate.Reset();

  network::ResourceRequest request2;
  request2.url = GURL("http://blocked.net");
  throttle.WillStartRequest(&request2, &defer);
  EXPECT_TRUE(delegate.canceled());
  EXPECT_EQ(delegate.cancel_reason(),
            "Resource load blocked by embedder policy.");
}

}  // namespace url_rewrite
