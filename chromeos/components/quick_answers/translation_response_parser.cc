// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace chromeos {
namespace quick_answers {

TranslationResponseParser::TranslationResponseParser(
    TranslationResponseParserCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {}

TranslationResponseParser::~TranslationResponseParser() {
  if (complete_callback_)
    std::move(complete_callback_).Run(/*quick_answer=*/nullptr);
}

void TranslationResponseParser::ProcessResponse(
    std::unique_ptr<std::string> response_body) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body->c_str(),
      base::BindOnce(&TranslationResponseParser::OnJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void TranslationResponseParser::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK(complete_callback_);

  if (!result.value) {
    LOG(ERROR) << "JSON parsing failed: " << *result.error;
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  auto* translations = result.value->FindListPath("data.translations");
  if (!translations) {
    LOG(ERROR) << "Can't find translations result list.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  DCHECK(translations->GetList().size() == 1);

  const std::string* translated_text =
      translations->GetList().front().FindStringPath("translatedText");
  if (!translated_text) {
    LOG(ERROR) << "Can't find a translated text.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }

  auto quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->result_type = ResultType::kTranslationResult;
  quick_answer->primary_answer = *translated_text;
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(*translated_text));

  std::move(complete_callback_).Run(std::move(quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
