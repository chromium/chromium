// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/ulp_language_model.h"

#include "base/check.h"

namespace language {

ULPLanguageModel::ULPLanguageModel() = default;

ULPLanguageModel::~ULPLanguageModel() = default;

std::vector<LanguageModel::LanguageDetails> ULPLanguageModel::GetLanguages() {
  return lang_details_;
}

void ULPLanguageModel::AddULPLanguage(std::string language, float score) {
  // Languages must be added in order by score.
  if (!lang_details_.empty()) {
    DCHECK(lang_details_.back().score >= score);
  }
  lang_details_.emplace_back(language, score);
}

}  // namespace language
