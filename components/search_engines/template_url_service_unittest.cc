// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <stddef.h>

#include <memory>

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLServiceUnitTest : public testing::Test {
 public:
  TemplateURLServiceUnitTest()
      : template_url_service_(/*initializers=*/nullptr, /*count=*/0) {}
  TemplateURLService& template_url_service() { return template_url_service_; }

 private:
  TemplateURLService template_url_service_;
};

TEST_F(TemplateURLServiceUnitTest, SessionToken) {
  // Subsequent calls always get the same token.
  std::string token = template_url_service().GetSessionToken();
  std::string token2 = template_url_service().GetSessionToken();
  EXPECT_EQ(token, token2);
  EXPECT_FALSE(token.empty());

  // Calls do not regenerate a token.
  template_url_service().current_token_ = "PRE-EXISTING TOKEN";
  token = template_url_service().GetSessionToken();
  EXPECT_EQ(token, "PRE-EXISTING TOKEN");

  // ... unless the token has expired.
  template_url_service().current_token_.clear();
  const base::TimeDelta kSmallDelta = base::Milliseconds(1);
  template_url_service().token_expiration_time_ =
      base::TimeTicks::Now() - kSmallDelta;
  token = template_url_service().GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, template_url_service().current_token_);

  // ... or cleared.
  template_url_service().current_token_.clear();
  template_url_service().ClearSessionToken();
  token = template_url_service().GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, template_url_service().current_token_);

  // The expiration time is always updated.
  template_url_service().GetSessionToken();
  base::TimeTicks expiration_time_1 =
      template_url_service().token_expiration_time_;
  base::PlatformThread::Sleep(kSmallDelta);
  template_url_service().GetSessionToken();
  base::TimeTicks expiration_time_2 =
      template_url_service().token_expiration_time_;
  EXPECT_GT(expiration_time_2, expiration_time_1);
  EXPECT_GE(expiration_time_2, expiration_time_1 + kSmallDelta);
}

TEST_F(TemplateURLServiceUnitTest, GenerateSearchURL) {
  // Set the default search provider to a custom one.
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_EQ(
      "https://www.example.com/?q=foo",
      template_url_service().GenerateSearchURLForDefaultSearchProvider(u"foo"));
  EXPECT_EQ(
      "https://www.example.com/?q=",
      template_url_service().GenerateSearchURLForDefaultSearchProvider(u""));
}
