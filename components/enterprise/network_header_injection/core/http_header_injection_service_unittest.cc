// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_matcher.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionServiceTest : public testing::Test {
 public:
  void SetUp() override { RegisterProfilePrefs(pref_service_.registry()); }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Tests that a newly constructed service with empty preferences has no rules
// and returns no custom headers.
TEST_F(HttpHeaderInjectionServiceTest, InitiallyEmpty) {
  HttpHeaderInjectionService service(&pref_service_);
  EXPECT_FALSE(service.HasRules());
  EXPECT_TRUE(service.GetHeadersForUrl(GURL("https://example.com")).IsEmpty());
}

// Tests that preferences already present in the pref store are successfully
// loaded and parsed upon service initialization.
TEST_F(HttpHeaderInjectionServiceTest, InitialPrefLoading) {
  base::ListValue rules;
  base::DictValue rule;
  base::ListValue patterns;
  patterns.Append("example.com");
  rule.Set(kKeyPatterns, std::move(patterns));

  base::ListValue headers;
  base::DictValue header;
  header.Set(kKeyHeaderName, "X-Test");
  header.Set(kKeyHeaderValue, "Value");
  headers.Append(std::move(header));
  rule.Set(kKeyHeaders, std::move(headers));

  rules.Append(std::move(rule));
  pref_service_.SetList(prefs::kHttpHeaderInjection, std::move(rules));

  HttpHeaderInjectionService service(&pref_service_);
  EXPECT_TRUE(service.HasRules());

  auto results = service.GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("Value", results.GetHeader("X-Test"));
}

// Tests that the service dynamically responds to pref store updates after
// construction, updating its injection rules on the fly.
TEST_F(HttpHeaderInjectionServiceTest, PrefsChangeDynamically) {
  HttpHeaderInjectionService service(&pref_service_);
  EXPECT_FALSE(service.HasRules());

  base::ListValue rules;
  base::DictValue rule;
  base::ListValue patterns;
  patterns.Append("example.com");
  rule.Set(kKeyPatterns, std::move(patterns));

  base::ListValue headers;
  base::DictValue header;
  header.Set(kKeyHeaderName, "X-Test");
  header.Set(kKeyHeaderValue, "Value");
  headers.Append(std::move(header));
  rule.Set(kKeyHeaders, std::move(headers));

  rules.Append(std::move(rule));
  pref_service_.SetList(prefs::kHttpHeaderInjection, std::move(rules));

  EXPECT_TRUE(service.HasRules());
  auto results = service.GetHeadersForUrl(GURL("https://example.com"));
  EXPECT_EQ("Value", results.GetHeader("X-Test"));

  // Clear rules.
  pref_service_.SetList(prefs::kHttpHeaderInjection, base::ListValue());
  EXPECT_FALSE(service.HasRules());
  EXPECT_TRUE(service.GetHeadersForUrl(GURL("https://example.com")).IsEmpty());
}

}  // namespace enterprise_custom_headers
