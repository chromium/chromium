// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/logging.h"
#include <sstream>

namespace segmentation_platform {

std::string PredictionResultToDebugString(
    proto::PredictionResult prediction_result) {
  std::stringstream debug_string;
  debug_string << "PredictionResult: timestamp: "
               << prediction_result.timestamp_us();

  for (auto i = 0; i < prediction_result.result_size(); ++i) {
    debug_string << " result " << i << ": " << prediction_result.result(i);
  }

  return debug_string.str();
}

}  // namespace segmentation_platform
