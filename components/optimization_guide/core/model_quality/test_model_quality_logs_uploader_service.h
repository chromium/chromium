// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_TEST_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_TEST_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

class PrefService;

namespace optimization_guide {

class MqlsFeatureMetadata;

// A test double for the ModelQualityLogsUploaderService, which stores uploaded
// logs in memory rather than uploading them to a server.
class TestModelQualityLogsUploaderService
    : public ModelQualityLogsUploaderService {
 public:
  explicit TestModelQualityLogsUploaderService(PrefService* pref_service);

  TestModelQualityLogsUploaderService(
      const TestModelQualityLogsUploaderService&) = delete;
  TestModelQualityLogsUploaderService& operator=(
      const TestModelQualityLogsUploaderService&) = delete;

  ~TestModelQualityLogsUploaderService() override;

  bool CanUploadLogs(const MqlsFeatureMetadata* metadata) override;

  // Sets a callback that will be run after the next log is uploaded.
  void WaitForLogUpload(base::OnceCallback<void()> callback);

  const std::vector<std::unique_ptr<proto::LogAiDataRequest>>& uploaded_logs()
      const {
    return uploaded_logs_;
  }

 private:
  void UploadFinalizedLog(
      std::unique_ptr<proto::LogAiDataRequest> log,
      proto::LogAiDataRequest::FeatureCase feature) override;

  // The list of "uploaded" logs (which are stored in memory rather than
  // actually uploaded), in FIFO order.
  std::vector<std::unique_ptr<proto::LogAiDataRequest>> uploaded_logs_;

  // If not null, will be run after the next log is uploaded.
  base::OnceCallback<void()> on_log_uploaded_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_TEST_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
