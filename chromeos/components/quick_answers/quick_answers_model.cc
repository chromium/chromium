// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace quick_answers {

PhoneticsInfo::PhoneticsInfo() = default;
PhoneticsInfo::PhoneticsInfo(const PhoneticsInfo&) = default;
PhoneticsInfo::~PhoneticsInfo() = default;

QuickAnswer::QuickAnswer() = default;
QuickAnswer::~QuickAnswer() = default;

IntentInfo::IntentInfo() = default;
IntentInfo::IntentInfo(const IntentInfo& other) = default;
IntentInfo::IntentInfo(const std::string& intent_text,
                       IntentType intent_type,
                       const std::string& device_language,
                       const std::string& source_language) {
  this->intent_text = intent_text;
  this->intent_type = intent_type;
  this->device_language = device_language;
  this->source_language = source_language;
}
IntentInfo::~IntentInfo() = default;

PreprocessedOutput::PreprocessedOutput() = default;
PreprocessedOutput::PreprocessedOutput(const PreprocessedOutput& other) =
    default;
PreprocessedOutput::~PreprocessedOutput() = default;

QuickAnswersRequest::QuickAnswersRequest() = default;
QuickAnswersRequest::QuickAnswersRequest(const QuickAnswersRequest& other) =
    default;
QuickAnswersRequest::~QuickAnswersRequest() = default;

TranslationResult::TranslationResult() = default;
TranslationResult::~TranslationResult() = default;

StructuredResult::StructuredResult() = default;
StructuredResult::~StructuredResult() = default;

QuickAnswersSession::QuickAnswersSession() = default;
QuickAnswersSession::~QuickAnswersSession() = default;

}  // namespace quick_answers
