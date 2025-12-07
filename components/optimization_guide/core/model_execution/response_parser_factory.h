// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_FACTORY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_FACTORY_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

std::unique_ptr<ResponseParser> CreateResponseParser(
    const proto::OnDeviceModelExecutionOutputConfig& output_config);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_FACTORY_H_
