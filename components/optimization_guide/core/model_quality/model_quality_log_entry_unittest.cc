// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class MqlsFeatureMetadata;

namespace {

std::unique_ptr<proto::LogAiDataRequest> BuildComposeLogAiDataRequest() {
  auto log = std::make_unique<proto::LogAiDataRequest>();
  proto::ComposeLoggingData compose_data;
  *(log->mutable_compose()) = compose_data;
  return log;
}

class TestModelQualityLogsUploaderService
    : public ModelQualityLogsUploaderService {
 public:
  explicit TestModelQualityLogsUploaderService(
      network::TestURLLoaderFactory* url_loader_factory,
      TestingPrefServiceSimple* prefs)
      : ModelQualityLogsUploaderService(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                url_loader_factory),
            prefs) {}

  ~TestModelQualityLogsUploaderService() override = default;

  TestModelQualityLogsUploaderService(
      const TestModelQualityLogsUploaderService&) = delete;
  TestModelQualityLogsUploaderService& operator=(
      const TestModelQualityLogsUploaderService&) = delete;

  bool CanUploadLogs(const MqlsFeatureMetadata*) override { return true; }
};

}  // namespace

class ModelQualityLogEntryTest : public testing::Test {
 public:
  ModelQualityLogEntryTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        uploader_(&url_loader_factory_, &prefs_) {}
  ~ModelQualityLogEntryTest() override = default;

  base::WeakPtr<ModelQualityLogsUploaderService> uploader() {
    return uploader_.GetWeakPtr();
  }

  void SetUp() override {
    model_execution::prefs::RegisterProfilePrefs(prefs_.registry());
    model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());
  }

  int NumPendingUploads() {
    RunUntilIdle();
    return url_loader_factory_.NumPending();
  }

 private:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple prefs_;
  TestModelQualityLogsUploaderService uploader_;
};

// Test ModelQualityLogEntry initialization and logging_metadata().
TEST_F(ModelQualityLogEntryTest, Initialize) {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      new proto::LogAiDataRequest());
  optimization_guide::proto::LoggingMetadata* logging_metadata =
      log_ai_data_request.get()->mutable_logging_metadata();

  ModelQualityLogEntry log_entry(std::move(log_ai_data_request), nullptr);

  EXPECT_EQ(log_entry.log_ai_data_request()->mutable_logging_metadata(),
            logging_metadata);
}

// Test client id is correctly set.
TEST_F(ModelQualityLogEntryTest, ClientId) {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      std::make_unique<proto::LogAiDataRequest>());
  int64_t client_id =
      log_ai_data_request.get()->mutable_logging_metadata()->client_id();

  ModelQualityLogEntry log_entry(std::move(log_ai_data_request), nullptr);

  EXPECT_EQ(
      log_entry.log_ai_data_request()->mutable_logging_metadata()->client_id(),
      client_id);
}

TEST_F(ModelQualityLogEntryTest, UploadOnDestruction) {
  {
    auto data = BuildComposeLogAiDataRequest();
    auto log =
        std::make_unique<ModelQualityLogEntry>(std::move(data), uploader());
  }  // ModelQualityLogEntry destroyed, this should trigger an upload.

  EXPECT_EQ(1, NumPendingUploads());
}

TEST_F(ModelQualityLogEntryTest, Upload_WithNonEmptyLog_SchedulesAnUpload) {
  auto data = BuildComposeLogAiDataRequest();
  auto log =
      std::make_unique<ModelQualityLogEntry>(std::move(data), uploader());

  ModelQualityLogEntry::Upload(std::move(log));

  EXPECT_EQ(1, NumPendingUploads());
}

TEST_F(ModelQualityLogEntryTest, Upload_WithEmptyLog_DoesNotScheduleAnUpload) {
  std::unique_ptr<proto::LogAiDataRequest> data;
  auto log =
      std::make_unique<ModelQualityLogEntry>(std::move(data), uploader());

  ModelQualityLogEntry::Upload(std::move(log));

  EXPECT_EQ(0, NumPendingUploads());
}

TEST_F(ModelQualityLogEntryTest, Upload_WithNullEntry_DoesNotCrash) {
  ModelQualityLogEntry::Upload(nullptr);
}

TEST_F(ModelQualityLogEntryTest, Drop_WithNonEmptyLog_DoesNotScheduleAnUpload) {
  auto data = BuildComposeLogAiDataRequest();
  auto log =
      std::make_unique<ModelQualityLogEntry>(std::move(data), uploader());

  ModelQualityLogEntry::Drop(std::move(log));

  EXPECT_EQ(0, NumPendingUploads());
}

TEST_F(ModelQualityLogEntryTest, Drop_WithNullEntry_DoesNotCrash) {
  ModelQualityLogEntry::Drop(nullptr);
}

}  // namespace optimization_guide
