// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
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
    std::unique_ptr<std::string> response_body) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body->c_str(),
      base::BindOnce(&TranslationResponseParser::OnJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void TranslationResponseParser::OnJsonParsed(
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

  DCHECK_EQ(translations->size(), 1ul);

  const std::string* translated_text_ptr =
      translations->front().GetDict().FindStringByDottedPath("translatedText");
  if (!translated_text_ptr) {
    LOG(ERROR) << "Can't find a translated text.";
    std::move(complete_callback_).Run(nullptr);
    return;
  }
  std::string translated_text = UnescapeStringForHTML(*translated_text_ptr);

  std::unique_ptr<TranslationResult> translation_result =
      std::make_unique<TranslationResult>();
  translation_result->translated_text = translated_text;
  std::move(complete_callback_).Run(std::move(translation_result));
}

}  // namespace quick_answers
