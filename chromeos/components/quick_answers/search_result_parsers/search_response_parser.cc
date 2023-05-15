// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace quick_answers {
namespace {

using base::Value;

// String to prepend to JSON responses to prevent XSSI. See http://go/xssi.
constexpr char kJsonSafetyPrefix[] = ")]}'\n";

}  // namespace

SearchResponseParser::SearchResponseParser(
    SearchResponseParserCallback complete_callback) {
  complete_callback_ = std::move(complete_callback);
}

SearchResponseParser::~SearchResponseParser() {
  if (complete_callback_)
    std::move(complete_callback_).Run(/*quick_answer=*/nullptr);
}

void SearchResponseParser::ProcessResponse(
    std::unique_ptr<std::string> response_body) {
  if (response_body->length() < strlen(kJsonSafetyPrefix) ||
      response_body->substr(0, strlen(kJsonSafetyPrefix)) !=
          kJsonSafetyPrefix) {
    LOG(ERROR) << "Invalid search response.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body->substr(strlen(kJsonSafetyPrefix)),
      base::BindOnce(&SearchResponseParser::OnJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void SearchResponseParser::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK(complete_callback_);

  if (!result.has_value()) {
    LOG(ERROR) << "JSON parsing failed: " << result.error();
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  // Get the first result.
  const Value::List* entries =
      result->GetDict().FindListByDottedPath("results");
  if (!entries) {
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  for (const auto& entry : *entries) {
    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        ProcessResult(&entry);
    if (quick_answers_session) {
      std::move(complete_callback_).Run(std::move(quick_answers_session));
      return;
    }
  }

  std::move(complete_callback_).Run(nullptr);
}

std::unique_ptr<QuickAnswersSession> SearchResponseParser::ProcessResult(
    const Value* result) {
  const base::Value::Dict& dict = result->GetDict();
  auto one_namespace_type = dict.FindInt("oneNamespaceType");
  if (!one_namespace_type.has_value()) {
    // Can't find valid one namespace type from the response.
    LOG(ERROR) << "Can't find valid one namespace type from the response.";
    return nullptr;
  }

  std::unique_ptr<ResultParser> result_parser =
      ResultParserFactory::Create(one_namespace_type.value());
  if (!result_parser) {
    return nullptr;
  }

  if (result_parser->SupportsNewInterface()) {
    // Try to parse from StructuredResult, which supports Rich Answers.
    std::unique_ptr<StructuredResult> structured_result =
        result_parser->ParseInStructuredResult(dict);
    if (!structured_result) {
      return nullptr;
    }

    std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
    if (!result_parser->PopulateQuickAnswer(*structured_result,
                                            quick_answer.get())) {
      return nullptr;
    }

    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        std::make_unique<QuickAnswersSession>();
    quick_answers_session->structured_result = std::move(structured_result);
    quick_answers_session->quick_answer = std::move(quick_answer);
    return quick_answers_session;
  }

  // If a parser does not support `StructuredResult`, falls back to `Parse`
  // method. This is for a parser which has not migrated to the new interfaces
  // yet.
  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  if (!result_parser->Parse(dict, quick_answer.get())) {
    return nullptr;
  }

  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);
  return quick_answers_session;
}

}  // namespace quick_answers
