// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

using ReportEvent = ::chrome::cros::reporting::proto::SaasUsageReportEvent;

class FakeSaasUsageReportSchedulerDelegate
    : public SaasUsageReportScheduler::Delegate {
 public:
  FakeSaasUsageReportSchedulerDelegate() = default;
  ~FakeSaasUsageReportSchedulerDelegate() override = default;

  void SetReadyStateChangedCallback(base::RepeatingClosure callback) override {
    callback_ = callback;
  }

  void SetIsReady(bool is_ready) {
    is_ready_ = is_ready;
    OnReadyStateChanged();
  }

  bool IsReady() override { return is_ready_; }

  void OnReadyStateChanged() {
    if (callback_) {
      callback_.Run();
    }
  }

 private:
  bool is_ready_ = false;
  base::RepeatingClosure callback_;
};

class FakeSaasUsageReportFactoryDelegate
    : public SaasUsageReportFactory::Delegate {
 public:
  FakeSaasUsageReportFactoryDelegate() = default;
  ~FakeSaasUsageReportFactoryDelegate() override = default;

  std::optional<std::string> GetProfileId() override { return "profile_id"; }

  bool IsProfileAffiliated() override { return true; }
};

class FakeSaasUsageReportUploader : public SaasUsageReportUploader {
 public:
  FakeSaasUsageReportUploader() = default;
  ~FakeSaasUsageReportUploader() override = default;

  void UploadReport(const ReportEvent& report,
                    base::OnceCallback<void(bool)> callback) override {
    upload_count_++;
    std::move(callback).Run(should_upload_successfully_);
  }

  int upload_count() const { return upload_count_; }

  void SetShouldUploadSuccessfully(bool should_upload_successfully) {
    should_upload_successfully_ = should_upload_successfully;
  }

 private:
  int upload_count_ = 0;
  bool should_upload_successfully_ = true;
};

}  // namespace

class SaasUsageReportSchedulerTest : public ::testing::Test {
 public:
  SaasUsageReportSchedulerTest() = default;
  ~SaasUsageReportSchedulerTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);
    pref_service_.registry()->RegisterTimePref(kSaasUsageReportLastTriggerTime,
                                               base::Time());
  }

  void CreateScheduler(std::unique_ptr<FakeSaasUsageReportSchedulerDelegate>
                           delegate = nullptr) {
    auto factory_delegate =
        std::make_unique<FakeSaasUsageReportFactoryDelegate>();
    auto report_factory = std::make_unique<SaasUsageReportFactory>(
        &pref_service_, std::move(factory_delegate));

    auto report_uploader = std::make_unique<FakeSaasUsageReportUploader>();
    report_uploader_ = report_uploader.get();

    scheduler_ = std::make_unique<SaasUsageReportScheduler>(
        &pref_service_, std::move(report_factory), std::move(report_uploader),
        std::move(delegate));
  }

  void RecordNavigation() {
    enterprise_reporting::RecordNavigation(pref_service_, "example.com",
                                           "TLS 1.3");
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SaasUsageReportScheduler> scheduler_;
  raw_ptr<FakeSaasUsageReportUploader> report_uploader_;
};

TEST_F(SaasUsageReportSchedulerTest,
       TriggersReportImmediatelyWhenNoLastTriggerTime) {
  RecordNavigation();
  CreateScheduler();

  task_environment_.FastForwardBy(base::TimeDelta());
  // The report should be triggered immediately on startup when there is no last
  // trigger time.
  EXPECT_EQ(1, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest,
       TriggersReportImmediatelyWhenLastTriggerTimeIsOlderThanInterval) {
  // Set the last trigger time to older than the interval.
  pref_service_.SetTime(
      kSaasUsageReportLastTriggerTime,
      base::Time::Now() - 2 * SaasUsageReportScheduler::kReportInterval);
  RecordNavigation();
  CreateScheduler();

  task_environment_.FastForwardBy(base::TimeDelta());
  // The report should be triggered immediately on startup because the last
  // trigger time is older than the interval.
  EXPECT_EQ(1, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest, TriggersReportPeriodically) {
  // Record a navigation to ensure that the report is not empty.
  RecordNavigation();
  CreateScheduler();

  // The report should be triggered immediately on startup.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, report_uploader_->upload_count());

  RecordNavigation();
  // Fast forward time by report interval hours, the report should be triggered.
  task_environment_.FastForwardBy(SaasUsageReportScheduler::kReportInterval);
  EXPECT_EQ(2, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest, NoImmediateTriggerWhenRecentlyTriggered) {
  pref_service_.SetTime(
      kSaasUsageReportLastTriggerTime,
      base::Time::Now() - SaasUsageReportScheduler::kReportInterval / 2);
  RecordNavigation();
  CreateScheduler();

  task_environment_.FastForwardBy(base::TimeDelta());
  // The report should not be triggered immediately on startup because the last
  // trigger time is less than the interval.
  EXPECT_EQ(0, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest,
       TriggersReportAfterRemainingTimeWhenRecentlyTriggered) {
  // Set the last trigger time to less than the interval.
  base::TimeDelta time_since_last_trigger =
      SaasUsageReportScheduler::kReportInterval / 2;
  pref_service_.SetTime(kSaasUsageReportLastTriggerTime,
                        base::Time::Now() - time_since_last_trigger);
  RecordNavigation();
  CreateScheduler();

  // The report should not be triggered immediately on startup.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0, report_uploader_->upload_count());

  // The report should be triggered after the remaining time.
  task_environment_.FastForwardBy(SaasUsageReportScheduler::kReportInterval -
                                  time_since_last_trigger);
  EXPECT_EQ(1, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest, DoesNotUploadEmptyReport) {
  // No navigation is recorded, so the report should be empty.
  CreateScheduler();
  // Immediate trigger should not upload an empty report.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0, report_uploader_->upload_count());

  // Periodic trigger should not upload an empty report.
  task_environment_.FastForwardBy(SaasUsageReportScheduler::kReportInterval);
  EXPECT_EQ(0, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest, ClearsReportPrefAfterSuccessfulUpload) {
  RecordNavigation();
  CreateScheduler();

  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(1, report_uploader_->upload_count());
  EXPECT_TRUE(pref_service_.GetDict(kSaasUsageReport).empty());
}

TEST_F(SaasUsageReportSchedulerTest, DoesNotClearReportPrefAfterFailedUpload) {
  RecordNavigation();
  CreateScheduler();
  report_uploader_->SetShouldUploadSuccessfully(false);

  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(1, report_uploader_->upload_count());
  EXPECT_FALSE(pref_service_.GetDict(kSaasUsageReport).empty());
}

TEST_F(SaasUsageReportSchedulerTest,
       UpdatesLastTriggerTimeWhenSuccessfulUpload) {
  RecordNavigation();
  CreateScheduler();

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(base::Time::Now(),
            pref_service_.GetTime(kSaasUsageReportLastTriggerTime));
}

TEST_F(SaasUsageReportSchedulerTest, UpdatesLastTriggerTimeWhenUploadFails) {
  RecordNavigation();
  CreateScheduler();
  report_uploader_->SetShouldUploadSuccessfully(false);

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(base::Time::Now(),
            pref_service_.GetTime(kSaasUsageReportLastTriggerTime));
}

TEST_F(SaasUsageReportSchedulerTest, TriggersOnlyWhenDelegateBecomesReady) {
  auto delegate = std::make_unique<FakeSaasUsageReportSchedulerDelegate>();
  auto* delegate_ptr = delegate.get();
  delegate_ptr->SetIsReady(false);
  RecordNavigation();
  CreateScheduler(std::move(delegate));
  task_environment_.FastForwardBy(base::TimeDelta());
  // The report should not be triggered because the delegate is not ready.
  EXPECT_EQ(0, report_uploader_->upload_count());

  delegate_ptr->SetIsReady(true);
  // The report should be triggered after the delegate becomes ready.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, report_uploader_->upload_count());
}

TEST_F(SaasUsageReportSchedulerTest,
       StopsTriggeringWhenDelegateBecomesNotReady) {
  auto delegate = std::make_unique<FakeSaasUsageReportSchedulerDelegate>();
  auto* delegate_ptr = delegate.get();
  delegate_ptr->SetIsReady(true);
  RecordNavigation();
  CreateScheduler(std::move(delegate));
  task_environment_.FastForwardBy(base::TimeDelta());
  // The report should be triggered because the delegate is ready.
  EXPECT_EQ(1, report_uploader_->upload_count());

  RecordNavigation();
  delegate_ptr->SetIsReady(false);
  task_environment_.FastForwardBy(SaasUsageReportScheduler::kReportInterval);
  // The upload count should not increase because the delegate is not ready.
  EXPECT_EQ(1, report_uploader_->upload_count());
}

}  // namespace enterprise_reporting
