// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/url_lookup_service.h"

#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class RealTimeUrlLookupServiceTest : public PlatformTest {
 public:
  RealTimeUrlLookupServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    rt_service_ =
        std::make_unique<RealTimeUrlLookupService>(test_shared_loader_factory_);
  }

  bool CanCheckUrl(const GURL& url) { return rt_service_->CanCheckUrl(url); }
  void HandleLookupError() { rt_service_->HandleLookupError(); }
  void HandleLookupSuccess() { rt_service_->HandleLookupSuccess(); }
  bool IsInBackoffMode() { return rt_service_->IsInBackoffMode(); }
  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url) {
    return rt_service_->FillRequestProto(url);
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RealTimeUrlLookupService> rt_service_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(RealTimeUrlLookupServiceTest, TestFillRequestProto) {
  struct SanitizeUrlCase {
    const char* url;
    const char* expected_url;
  } sanitize_url_cases[] = {
      {"http://example.com/", "http://example.com/"},
      {"http://user:pass@example.com/", "http://example.com/"},
      {"http://%123:bar@example.com/", "http://example.com/"},
      {"http://example.com#123", "http://example.com/"}};
  for (size_t i = 0; i < base::size(sanitize_url_cases); i++) {
    GURL url(sanitize_url_cases[i].url);
    auto result = FillRequestProto(url);
    EXPECT_EQ(sanitize_url_cases[i].expected_url, result->url());
    EXPECT_EQ(RTLookupRequest::NAVIGATION, result->lookup_type());
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffAndTimerReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff not reset after 299 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(298));
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff should have been reset after 300 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffAndLookupSuccessReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  HandleLookupError();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  HandleLookupError();
  EXPECT_TRUE(IsInBackoffMode());

  // Lookup success resets the backoff counter.
  HandleLookupSuccess();
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestGetSBThreatTypeForRTThreatType) {
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::WEB_MALWARE));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING));
  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE));
  EXPECT_EQ(SB_THREAT_TYPE_BILLING,
            RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNCLEAR_BILLING));
}

TEST_F(RealTimeUrlLookupServiceTest, TestCanCheckUrl) {
  struct CanCheckUrlCases {
    const char* url;
    bool can_check;
  } can_check_url_cases[] = {{"ftp://example.test/path", false},
                             {"http://localhost/path", false},
                             {"http://localhost.localdomain/path", false},
                             {"http://127.0.0.1/path", false},
                             {"http://127.0.0.1:2222/path", false},
                             {"http://192.168.1.1/path", false},
                             {"http://172.16.2.2/path", false},
                             {"http://10.1.1.1/path", false},
                             {"http://10.1.1.1.1/path", true},
                             {"http://example.test/path", true},
                             {"https://example.test/path", true}};
  for (size_t i = 0; i < base::size(can_check_url_cases); i++) {
    GURL url(can_check_url_cases[i].url);
    bool expected_can_check = can_check_url_cases[i].can_check;
    EXPECT_EQ(expected_can_check, CanCheckUrl(url));
  }
}

}  // namespace safe_browsing
