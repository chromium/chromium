// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_FCP_LOG_MANAGER_H_
#define COMPONENTS_PRIVATE_INSIGHTS_FCP_LOG_MANAGER_H_

#include <cstdint>
#include <string>

#include "third_party/federated_compute/src/fcp/client/log_manager.h"

namespace private_insights {

class FcpLogManager : public fcp::client::LogManager {
 public:
  FcpLogManager();
  ~FcpLogManager() override;

  void LogDiag(fcp::client::ProdDiagCode diag_code) override;
  void LogDiag(fcp::client::DebugDiagCode diag_code) override;
  void LogToLongHistogram(fcp::client::HistogramCounters histogram_counter,
                          int execution_index,
                          int epoch_index,
                          fcp::client::engine::DataSourceType data_source_type,
                          int64_t value) override;
  void SetModelIdentifier(const std::string& model_identifier) override;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_FCP_LOG_MANAGER_H_
