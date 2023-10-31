// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

#include "base/check.h"

namespace optimization_guide {

ModelQualityLogEntry::ModelQualityLogEntry(
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request)
    : log_ai_data_request_(std::move(log_ai_data_request)) {}

ModelQualityLogEntry::~ModelQualityLogEntry() {
  // TODO(b/301301447): Uploads log through ModelQualityService on destruction
  // if not uploaded.
}

}  // namespace optimization_guide
