// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_BASELINE_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_BASELINE_LANGUAGE_MODEL_H_

#include <string>
#include <vector>

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/url_language_histogram.h"

class PrefService;

namespace language {

// A language model that attempts to approximate Chrome's legacy behaviour as
// much as possible.
//
// Produces a list of languages in the following order:
//  1) Chrome's UI language,
//  2) The most frequently occuring languages in visited URLs (if they surpass a
//     given frequency threshold),
//  3) The user's accept languages.
//
// No code appears twice in the list. Codes with region subtags may appear in
// the list and are not backed-off. Entries are scored by inverse rank (i.e. 1,
// 1/2, 1/3, ...).
//
// NOTE: This model reads URL language histogram and accept language information
//       from user preferences. Hence, both sources must be registered on the
//       PrefService passed in at construction time.
class BaselineLanguageModel : public LanguageModel {
 public:
  BaselineLanguageModel(PrefService* pref_service,
                        const std::string& ui_lang,
                        const std::string& accept_langs_pref);

  // LanguageModel implementation.
  std::vector<LanguageDetails> GetLanguages() override;

 private:
  const PrefService* const pref_service_;
  const std::string ui_lang_;
  const std::string accept_langs_pref_;
  const UrlLanguageHistogram lang_histogram_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_BASELINE_LANGUAGE_MODEL_H_
