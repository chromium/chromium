// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"

#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

TestModelQualityLogsUploaderService::TestModelQualityLogsUploaderService(
    PrefService* pref_service)
    : ModelQualityLogsUploaderService(nullptr, pref_service) {}

TestModelQualityLogsUploaderService::~TestModelQualityLogsUploaderService() =
    default;

bool TestModelQualityLogsUploaderService::CanUploadLogs(
    const MqlsFeatureMetadata* metadata) {
  return true;
}

void TestModelQualityLogsUploaderService::WaitForLogUpload(
    base::OnceCallback<void()> callback) {
  on_log_uploaded_ = std::move(callback);
}

void TestModelQualityLogsUploaderService::UploadFinalizedLog(
    std::unique_ptr<proto::LogAiDataRequest> log,
    proto::LogAiDataRequest::FeatureCase feature) {
  uploaded_logs_.push_back(std::move(log));

  if (!on_log_uploaded_.is_null()) {
    std::move(on_log_uploaded_).Run();
  }
}

}  // namespace optimization_guide
