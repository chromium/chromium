// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_H_

#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"

namespace language {

// Defines a user language model represented by a ranked list of languages and
// associated scores.
class LanguageModel {
 public:
  // Information about one language that a user understands.
  struct LanguageDetails {
    LanguageDetails();
    LanguageDetails(const std::string& in_lang_code, float in_score);

    // The language code.
    std::string lang_code;

    // A score representing the importance of the language to the user. Higher
    // scores mean that the language is of more importance to the user.
    float score;
  };

  virtual ~LanguageModel() = default;

  // The set of languages that the user understands. The languages are ranked
  // from most important to least.
  virtual std::vector<LanguageDetails> GetLanguages() = 0;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_H_
