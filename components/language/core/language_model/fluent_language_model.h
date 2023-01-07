// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_FLUENT_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_FLUENT_LANGUAGE_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "components/language/core/browser/language_model.h"

class PrefService;
namespace translate {
class TranslatePrefs;
}

namespace language {

// A language model that returns the user's fluent languages.
class FluentLanguageModel : public LanguageModel {
 public:
  explicit FluentLanguageModel(PrefService* pref_service);
  ~FluentLanguageModel() override;

  // LanguageModel implementation.
  std::vector<LanguageDetails> GetLanguages() override;

 private:
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_LANGUAGE_MODEL_FLUENT_LANGUAGE_MODEL_H_
