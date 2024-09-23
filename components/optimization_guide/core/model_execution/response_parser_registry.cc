// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/response_parser_registry.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/core/model_execution/aqa_response_parser.h"
#include "components/optimization_guide/core/model_execution/json_response_parser.h"
#include "components/optimization_guide/core/model_execution/simple_response_parser.h"

namespace optimization_guide {

ResponseParserRegistry::ResponseParserRegistry() {
  factories_.emplace(proto::PARSER_KIND_UNSPECIFIED,
                     std::make_unique<SimpleResponseParserFactory>());
  factories_.emplace(proto::PARSER_KIND_SIMPLE,
                     std::make_unique<SimpleResponseParserFactory>());
  factories_.emplace(proto::PARSER_KIND_JSON,
                     std::make_unique<JsonResponseParserFactory>());
  factories_.emplace(proto::PARSER_KIND_AQA,
                     std::make_unique<AqaResponseParserFactory>());
}
ResponseParserRegistry::~ResponseParserRegistry() = default;

const ResponseParserRegistry& ResponseParserRegistry::Get() {
  static const base::NoDestructor<ResponseParserRegistry> instance;
  return *instance;
}

std::unique_ptr<ResponseParser> ResponseParserRegistry::CreateParser(
    const proto::OnDeviceModelExecutionOutputConfig& config) const {
  auto it = factories_.find(config.parser_kind());
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second->CreateParser(config);
}

}  // namespace optimization_guide
