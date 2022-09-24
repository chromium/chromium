// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
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
                     base::Unretained(this)));
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
  const Value* entries = result->FindListPath("results");
  if (!entries) {
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  for (const auto& entry : entries->GetList()) {
    auto quick_answer = std::make_unique<QuickAnswer>();
    if (ProcessResult(&entry, quick_answer.get())) {
      std::move(complete_callback_).Run(std::move(quick_answer));
      return;
    }
  }

  std::move(complete_callback_).Run(nullptr);
}

bool SearchResponseParser::ProcessResult(const Value* result,
                                         QuickAnswer* quick_answer) {
  auto one_namespace_type = result->FindIntPath("oneNamespaceType");
  if (!one_namespace_type.has_value()) {
    // Can't find valid one namespace type from the response.
    LOG(ERROR) << "Can't find valid one namespace type from the response.";
    return false;
  }

  std::unique_ptr<ResultParser> result_parser =
      ResultParserFactory::Create(one_namespace_type.value());
  if (!result_parser)
    return false;

  return result_parser->Parse(result, quick_answer);
}

}  // namespace quick_answers
