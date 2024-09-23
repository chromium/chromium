// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_ACCEPT_LANGUAGES_SERVICE_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_ACCEPT_LANGUAGES_SERVICE_H_

#include <set>
#include <string>
#include <string_view>

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace language {

// AcceptLanguagesService tracks the value of the "Accept-Language" HTTP
// header.
class AcceptLanguagesService : public KeyedService {
 public:
  // |accept_languages_pref| is the path to the preference storing the accept
  // languages.
  AcceptLanguagesService(PrefService* prefs, const char* accept_languages_pref);

  AcceptLanguagesService(const AcceptLanguagesService&) = delete;
  AcceptLanguagesService& operator=(const AcceptLanguagesService&) = delete;

  ~AcceptLanguagesService() override;

  // Returns true if |language| is available as Accept-Languages for the given
  // |display_locale|. |language| will be converted if it has the synonym of
  // accept language.
  static bool CanBeAcceptLanguage(std::string_view language);

  // Returns true if the passed language has been configured by the user as an
  // accept language. |language| will be converted if it has the synonym of
  // accept languages.
  bool IsAcceptLanguage(std::string_view language) const;

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

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_ACCEPT_LANGUAGES_SERVICE_H_
