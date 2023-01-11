// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

namespace quick_answers {

TranslationResponseParser::TranslationResponseParser(
    TranslationResponseParserCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {}

TranslationResponseParser::~TranslationResponseParser() {
  if (complete_callback_)
    std::move(complete_callback_).Run(/*quick_answer=*/nullptr);
}

void TranslationResponseParser::ProcessResponse(
    std::unique_ptr<std::string> response_body,
    const std::string& title_text) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body->c_str(),
      base::BindOnce(&TranslationResponseParser::OnJsonParsed,
                     weak_factory_.GetWeakPtr(), title_text));
}

void TranslationResponseParser::OnJsonParsed(
    const std::string& title_text,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK(complete_callback_);

  if (!result.has_value()) {
    LOG(ERROR) << "JSON parsing failed: " << result.error();
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  auto* translations =
      result->GetDict().FindListByDottedPath("data.translations");
  if (!translations) {
    LOG(ERROR) << "Can't find translations result list.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  DCHECK(translations->size() == 1);

  const std::string* translated_text_ptr =
      translations->front().GetDict().FindStringByDottedPath("translatedText");
  if (!translated_text_ptr) {
    LOG(ERROR) << "Can't find a translated text.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }
  auto translated_text = UnescapeStringForHTML(*translated_text_ptr);

  auto quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->result_type = ResultType::kTranslationResult;
  quick_answer->title.push_back(std::make_unique<QuickAnswerText>(title_text));
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(translated_text));

  std::move(complete_callback_).Run(std::move(quick_answer));
}

}  // namespace quick_answers
