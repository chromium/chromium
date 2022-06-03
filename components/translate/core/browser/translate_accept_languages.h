// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_H_

#include <set>
#include <string>

#include "base/strings/string_piece.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace translate {

// TranslateAcceptLanguages tracks the value of the "Accept-Language" HTTP
// header.
class TranslateAcceptLanguages : public KeyedService {
 public:
  // |accept_languages_pref| is the path to the preference storing the accept
  // languages.
  TranslateAcceptLanguages(PrefService* prefs,
                           const char* accept_languages_pref);

  TranslateAcceptLanguages(const TranslateAcceptLanguages&) = delete;
  TranslateAcceptLanguages& operator=(const TranslateAcceptLanguages&) = delete;

  ~TranslateAcceptLanguages() override;

  // Returns true if |language| is available as Accept-Languages. |language|
  // will be converted if it has the synonym of accept language.
  static bool CanBeAcceptLanguage(base::StringPiece language);

  // Returns true if the passed language has been configured by the user as an
  // accept language. |language| will be converted if it has the synonym of
  // accept languages.
  bool IsAcceptLanguage(base::StringPiece language) const;

 private:
  // Initializes the |accept_languages_| language table based on the associated
  // preference in |prefs|.
  void InitAcceptLanguages(PrefService* prefs);

  // Set of accept languages.
  std::set<std::string> accept_languages_;

  // Listens to accept languages changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Path of accept languages preference.
  const std::string accept_languages_pref_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_H_
