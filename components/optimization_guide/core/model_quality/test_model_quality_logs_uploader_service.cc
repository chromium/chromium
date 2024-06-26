// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

TestModelQualityLogsUploaderService::TestModelQualityLogsUploaderService(
    PrefService* pref_service)
    : ModelQualityLogsUploaderService(nullptr, pref_service) {}

TestModelQualityLogsUploaderService::~TestModelQualityLogsUploaderService() =
    default;

bool TestModelQualityLogsUploaderService::CanUploadLogs(
    UserVisibleFeatureKey feature) {
  return true;
}

void TestModelQualityLogsUploaderService::UploadFinalizedLog(
    std::unique_ptr<proto::LogAiDataRequest> log,
    UserVisibleFeatureKey feature) {
  uploaded_logs_.push_back(std::move(log));
}

}  // namespace optimization_guide
