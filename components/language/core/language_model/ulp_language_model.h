// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_ULP_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_ULP_LANGUAGE_MODEL_H_

#include <string>
#include <vector>

#include "components/language/core/browser/language_model.h"

namespace language {

// A language model that returns the user's top ULP language preferences.
class ULPLanguageModel : public LanguageModel {
 public:
  explicit ULPLanguageModel();
  ~ULPLanguageModel() override;

  // LanguageModel implementation.
  std::vector<LanguageDetails> GetLanguages() override;

  void AddULPLanguage(std::string language, float score);

 private:
  std::vector<LanguageDetails> lang_details_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_ULP_LANGUAGE_MODEL_H_
