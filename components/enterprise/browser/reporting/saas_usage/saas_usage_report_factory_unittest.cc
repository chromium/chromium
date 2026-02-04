// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

class FakeDelegate : public SaasUsageReportFactory::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  std::optional<std::string> GetProfileId() override { return profile_id_; }
  bool IsProfileAffiliated() override { return is_affiliated_; }

  void SetProfileId(std::optional<std::string> profile_id) {
    profile_id_ = profile_id;
  }
  void SetIsAffiliated(bool is_affiliated) { is_affiliated_ = is_affiliated; }

 private:
  std::optional<std::string> profile_id_;
  bool is_affiliated_ = false;
};

class SaasUsageReportFactoryTest : public ::testing::Test {
 public:
  SaasUsageReportFactoryTest() {
    pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);
    auto fake_delegate = std::make_unique<FakeDelegate>();
    fake_delegate_ = fake_delegate.get();
    factory_ = std::make_unique<SaasUsageReportFactory>(
        &pref_service_, std::move(fake_delegate));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SaasUsageReportFactory> factory_;
  raw_ptr<FakeDelegate> fake_delegate_;
};

TEST_F(SaasUsageReportFactoryTest, CreateReportWithAffiliatedProfile) {
  const std::string kProfileId = "profile_id";
  fake_delegate_->SetProfileId(kProfileId);
  fake_delegate_->SetIsAffiliated(true);

  auto report = factory_->CreateReport();

  EXPECT_EQ(kProfileId, report.profile_id());
  EXPECT_TRUE(report.profile_affiliated());
}

TEST_F(SaasUsageReportFactoryTest, CreateReportWithUnaffiliatedProfile) {
  const std::string kProfileId = "profile_id";
  fake_delegate_->SetProfileId(kProfileId);
  fake_delegate_->SetIsAffiliated(false);

  auto report = factory_->CreateReport();

  EXPECT_EQ(kProfileId, report.profile_id());
  EXPECT_FALSE(report.profile_affiliated());
}

TEST_F(SaasUsageReportFactoryTest, CreateReportWithNoProfileInfo) {
  fake_delegate_->SetProfileId(std::nullopt);

  auto report = factory_->CreateReport();

  EXPECT_EQ("", report.profile_id());
}

TEST_F(SaasUsageReportFactoryTest, CreateReportWithDomainMetrics) {
  RecordNavigation(pref_service_, "google.com", "TLS 1.3");
  auto expected_time = base::Time::Now();

  auto report = factory_->CreateReport();

  ASSERT_EQ(1, report.domain_metrics_size());
  const auto& domain_metric = report.domain_metrics(0);
  EXPECT_EQ("google.com", domain_metric.domain());
  EXPECT_EQ(1u, domain_metric.visit_count());
  EXPECT_THAT(domain_metric.encryption_protocols(),
              testing::ElementsAre("TLS 1.3"));
  EXPECT_EQ(expected_time.InMillisecondsSinceUnixEpoch(),
            domain_metric.start_time_millis());
  EXPECT_EQ(expected_time.InMillisecondsSinceUnixEpoch(),
            domain_metric.end_time_millis());
}

}  // namespace enterprise_reporting
