// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"
#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

namespace quick_answers {
namespace {
using base::Value;

const constexpr char* kKnownHTMLTags[] = {"<b>", "</b>", "<i>", "</i>"};

}  // namespace

const Value::Dict* ResultParser::GetFirstDictElementFromList(
    const Value::Dict& dict,
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
  return &(entries->front().GetDict());
}

std::string ResultParser::RemoveKnownHtmlTags(const std::string& input) {
  // Copy input string to another string so we don't modify the passed value.
  std::string out = input;
  for (const char* html_tag : kKnownHTMLTags) {
    base::ReplaceSubstringsAfterOffset(&out, /*start_offset=*/0, html_tag, "");
  }
  return out;
}

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

// static
std::unique_ptr<ResultParser> ResultParserFactory::Create(
    int one_namespace_type) {
  switch (static_cast<ResultType>(one_namespace_type)) {
    // TODO(b/345551832): delete KpEntityResultParser
    case ResultType::kDefinitionResult:
      return std::make_unique<DefinitionResultParser>();
    case ResultType::kUnitConversionResult:
      return std::make_unique<UnitConversionResultParser>();
    // Translation responses are from the Clound server and parsed
    // separately.
    case ResultType::kTranslationResult:
    case ResultType::kNoResult:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return nullptr;
}

}  // namespace quick_answers
