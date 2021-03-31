// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_FLUENT_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_FLUENT_LANGUAGE_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "components/language/core/browser/language_model.h"

class PrefService;

namespace language {

class LanguagePrefs;

// A language model that returns the user's fluent languages.
class FluentLanguageModel : public LanguageModel {
 public:
  FluentLanguageModel(PrefService* pref_service,
                      const std::string& accept_langs_pref);
  ~FluentLanguageModel() override;

  // LanguageModel implementation.
  std::vector<LanguageDetails> GetLanguages() override;

 private:
  const PrefService* const pref_service_;
  const std::string accept_langs_pref_;
  std::unique_ptr<LanguagePrefs> language_prefs_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_FLUENT_LANGUAGE_MODEL_H_
