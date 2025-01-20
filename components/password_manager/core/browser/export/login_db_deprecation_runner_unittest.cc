// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_runner.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter_interface.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
using ::testing::WithArgs;

const std::string kExportProgressHistogram =
    "PasswordManager.UPM.LoginDbDeprecationExport.Progress";

class MockLoginDbDeprecationPasswordExporter
    : public LoginDbDeprecationPasswordExporterInterface {
 public:
  MOCK_METHOD(void,
              Start,
              (scoped_refptr<PasswordStoreInterface> password_store,
               base::OnceClosure export_cleanup_calback),
              (override));
};
}  // namespace

class LoginDbDeprecationdRunnerTest : public testing::Test {
 public:
  LoginDbDeprecationdRunnerTest() = default;

  void SetUp() override {
    std::unique_ptr<MockLoginDbDeprecationPasswordExporter>
        exporter_unique_ptr =
            std::make_unique<MockLoginDbDeprecationPasswordExporter>();
    mock_exporter_ = exporter_unique_ptr.get();
    db_export_runner_ = std::make_unique<LoginDbDeprecationRunner>(
        std::move(exporter_unique_ptr));
    mock_password_store_ = base::MakeRefCounted<MockPasswordStoreInterface>();
  }

  LoginDbDeprecationRunner* db_export_runner() {
    return db_export_runner_.get();
  }

  MockLoginDbDeprecationPasswordExporter* mock_exporter() {
    return mock_exporter_;
  }

  scoped_refptr<MockPasswordStoreInterface> mock_password_store() {
    return mock_password_store_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  raw_ptr<MockLoginDbDeprecationPasswordExporter> mock_exporter_;
  std::unique_ptr<LoginDbDeprecationRunner> db_export_runner_;
  scoped_refptr<MockPasswordStoreInterface> mock_password_store_;
};

TEST_F(LoginDbDeprecationdRunnerTest, ExportScheduledNotStartingBeforeDelay) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kLoginDbDeprecationAndroid,
      {{features::kLoginDbDeprecationExportDelay.name, "5"}});
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_exporter(), Start).Times(0);
  db_export_runner()->StartExportWithDelay(mock_password_store());

  // Fast forward by a less time than the task delay.
  task_env_.FastForwardBy(base::Seconds(3));
  histogram_tester.ExpectUniqueSample(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kScheduled,
      1);
}

TEST_F(LoginDbDeprecationdRunnerTest, ExportRunsAfterDelayButDoesntFinish) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kLoginDbDeprecationAndroid,
      {{features::kLoginDbDeprecationExportDelay.name, "5"}});

  base::HistogramTester histogram_tester;

  db_export_runner()->StartExportWithDelay(mock_password_store());
  EXPECT_CALL(*mock_exporter(), Start);
  task_env_.FastForwardBy(base::Seconds(5));

  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kScheduled,
      1);
  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kStarted, 1);
  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kFinished, 0);
}

TEST_F(LoginDbDeprecationdRunnerTest, ExportRunsAndFinishes) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kLoginDbDeprecationAndroid,
      {{features::kLoginDbDeprecationExportDelay.name, "5"}});

  base::HistogramTester histogram_tester;

  db_export_runner()->StartExportWithDelay(mock_password_store());
  EXPECT_CALL(*mock_exporter(), Start)
      .WillOnce(WithArgs<1>([](base::OnceClosure completion_callback) {
        std::move(completion_callback).Run();
      }));
  task_env_.FastForwardBy(base::Seconds(5));

  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kScheduled,
      1);
  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kStarted, 1);
  histogram_tester.ExpectBucketCount(
      kExportProgressHistogram, LoginDbDeprecationExportProgress::kFinished, 1);
}

}  // namespace password_manager
