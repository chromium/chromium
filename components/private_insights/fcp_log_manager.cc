// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_log_manager.h"

#include "base/logging.h"

namespace private_insights {

FcpLogManager::FcpLogManager() = default;
FcpLogManager::~FcpLogManager() = default;

void FcpLogManager::LogDiag(fcp::client::ProdDiagCode diag_code) {
  DVLOG(4) << "FCP LogDiag (prod): " << static_cast<int>(diag_code);
}

void FcpLogManager::LogDiag(fcp::client::DebugDiagCode diag_code) {
  DVLOG(4) << "FCP LogDiag (debug): " << static_cast<int>(diag_code);
}

void FcpLogManager::LogToLongHistogram(
    fcp::client::HistogramCounters histogram_counter,
    int execution_index,
    int epoch_index,
    fcp::client::engine::DataSourceType data_source_type,
    int64_t value) {}

void FcpLogManager::SetModelIdentifier(const std::string& model_identifier) {}

}  // namespace private_insights
