// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/data_protection_url_lookup_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUrl[] = "someurl.com";

safe_browsing::RTLookupResponse::ThreatInfo GetTestThreatInfo(
    int cache_duration_sec) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  threat_info.set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  threat_info.set_cache_duration_sec(cache_duration_sec);
  return threat_info;
}

std::unique_ptr<safe_browsing::RTLookupResponse> CreateRTLookupResponse(
    int cache_duration_sec) {
  auto response = std::make_unique<safe_browsing::RTLookupResponse>();
  safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
      response->add_threat_info();
  *new_threat_info = GetTestThreatInfo(cache_duration_sec);
  return response;
}

class MockRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  MOCK_METHOD(void, LookupCalled, ());

  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    LookupCalled();
    std::move(response_callback)
        .Run(true, true, CreateRTLookupResponse(cache_duration_sec_));
  }

  void set_cache_duration_sec(int cache_duration_sec) {
    cache_duration_sec_ = cache_duration_sec;
  }

 private:
  int cache_duration_sec_ = 0;
};

}  // namespace

namespace enterprise_data_protection {

struct UrlLookupTestCase {
  int cache_duration_sec;
  int second_do_lookup_delay_sec;
  int do_lookup_call_count;
};

UrlLookupTestCase kUrlLookupTestCases[] = {{.cache_duration_sec = 90,
                                            .second_do_lookup_delay_sec = 0,
                                            .do_lookup_call_count = 1},
                                           {.cache_duration_sec = 90,
                                            .second_do_lookup_delay_sec = 100,
                                            .do_lookup_call_count = 2}};

class DataProtectionUrlLookupServiceTest
    : public testing::Test,
      public testing::WithParamInterface<UrlLookupTestCase> {
 public:
  DataProtectionUrlLookupServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void CreateLookupService() {
    service_ = std::make_unique<DataProtectionUrlLookupService>();
  }

  DataProtectionUrlLookupService* GetLookupService() {
    if (!service_) {
      CreateLookupService();
    }
    return service_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DataProtectionUrlLookupService> service_;
};

TEST_P(DataProtectionUrlLookupServiceTest, VerdictCachePopulated) {
  UrlLookupTestCase test_case = GetParam();

  // create the mock lookup
  MockRealTimeUrlLookupService lookup_service;
  lookup_service.set_cache_duration_sec(test_case.cache_duration_sec);
  EXPECT_CALL(lookup_service, LookupCalled)
      .Times(test_case.do_lookup_call_count);

  // create the service
  auto* dp_lookup_service = GetLookupService();

  // call DoLookup, passing the fake
  GURL url(kUrl);
  SessionID session_id = SessionID::FromSerializedValue(1);
  dp_lookup_service->DoLookup(&lookup_service, url, base::DoNothing(),
                              session_id);

  task_environment_.FastForwardBy(
      base::Seconds(test_case.second_do_lookup_delay_sec));

  // second call to the same url ensure value is fetched from cache
  dp_lookup_service->DoLookup(&lookup_service, url, base::DoNothing(),
                              session_id);
}

INSTANTIATE_TEST_SUITE_P(,
                         DataProtectionUrlLookupServiceTest,
                         testing::ValuesIn(kUrlLookupTestCases));

}  // namespace enterprise_data_protection
