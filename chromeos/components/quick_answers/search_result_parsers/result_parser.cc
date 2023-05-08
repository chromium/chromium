// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

namespace quick_answers {
namespace {
using base::Value;
}  // namespace

std::unique_ptr<StructuredResult> ResultParser::ParseInStructuredResult(
    const base::Value::Dict& result) {
  return nullptr;
}

bool ResultParser::PopulateQuickAnswer(
    const StructuredResult& structured_result,
    QuickAnswer* quick_answer) {
  return false;
}

bool ResultParser::SupportsNewInterface() const {
  return false;
}

const Value::Dict* ResultParser::GetFirstListElement(const Value::Dict& dict,
                                                     const std::string& path) {
  const Value::List* entries = dict.FindListByDottedPath(path);

  if (!entries) {
    // No list found.
    return nullptr;
  }

  if (entries->empty()) {
    // No valid dictionary entries found.
    return nullptr;
  }
  return &(*entries)[0].GetDict();
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
