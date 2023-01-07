// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

namespace quick_answers {
namespace {
using base::Value;
}  // namespace

const Value* ResultParser::GetFirstListElement(const Value& value,
                                               const std::string& path) {
  const Value* entries = value.FindListPath(path);

  if (!entries) {
    // No list found.
    return nullptr;
  }

  const auto& list = entries->GetList();
  if (list.empty()) {
    // No valid dictionary entries found.
    return nullptr;
  }
  return &list[0];
}

// static
std::unique_ptr<ResultParser> ResultParserFactory::Create(
    int one_namespace_type) {
  switch (static_cast<ResultType>(one_namespace_type)) {
    case ResultType::kKnowledgePanelEntityResult:
      return std::make_unique<KpEntityResultParser>();
    case ResultType::kDefinitionResult:
      return std::make_unique<DefinitionResultParser>();
    case ResultType::kUnitConversionResult:
      return std::make_unique<UnitConversionResultParser>();
      // TODO(llin): Add other result parsers.

    // Translation responses are from the Clound server and parsed
    // separately.
    case ResultType::kTranslationResult:
    case ResultType::kNoResult:
      NOTREACHED();
      break;
  }

  return nullptr;
}

}  // namespace quick_answers
