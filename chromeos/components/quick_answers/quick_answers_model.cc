// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

#include "base/strings/stringprintf.h"

namespace chromeos {
namespace quick_answers {

QuickAnswer::QuickAnswer() = default;
QuickAnswer::~QuickAnswer() = default;

IntentInfo::IntentInfo() = default;
IntentInfo::IntentInfo(const IntentInfo& other) = default;
IntentInfo::IntentInfo(const std::string& intent_text,
                       IntentType intent_type,
                       const std::string& source_language,
                       const std::string& target_language) {
  this->intent_text = intent_text;
  this->intent_type = intent_type;
  this->source_language = source_language;
  this->target_language = target_language;
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

}  // namespace quick_answers
}  // namespace chromeos
