// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/translation_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

namespace chromeos {
namespace quick_answers {
namespace {
using base::Value;

constexpr char kSourceTextPath[] = "translateResult.sourceText";
constexpr char kSourceLanguageLocalizedNamePath[] =
    "translateResult.sourceTextLanguageLocalizedName";
constexpr char kTranslatedTextPath[] = "translateResult.translatedText";

}  // namespace

// Extract |quick_answer| from translation result.
bool TranslationResultParser::Parse(const Value* result,
                                    QuickAnswer* quick_answer) {
  const std::string* source_text = result->FindStringPath(kSourceTextPath);
  if (!source_text) {
    LOG(ERROR) << "Can't find source text.";
    return false;
  }

  const std::string* source_text_language_localized_name =
      result->FindStringPath(kSourceLanguageLocalizedNamePath);
  if (!source_text_language_localized_name) {
    LOG(ERROR) << "Can't find a source text language localized name.";
    return false;
  }

  const std::string* translated_text =
      result->FindStringPath(kTranslatedTextPath);
  if (!translated_text) {
    LOG(ERROR) << "Can't find a translated text.";
    return false;
  }
  const std::string& secondary_answer = BuildTranslationTitleText(
      source_text->c_str(), source_text_language_localized_name->c_str());
  quick_answer->result_type = ResultType::kTranslationResult;
  quick_answer->primary_answer = *translated_text;
  quick_answer->secondary_answer = secondary_answer;
  quick_answer->title.push_back(
      std::make_unique<QuickAnswerText>(secondary_answer));
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(*translated_text));
  return true;
}

}  // namespace quick_answers
}  // namespace chromeos
