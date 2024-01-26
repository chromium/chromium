// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_
#define COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"

class HostContentSettingsMap;
class PrefService;

namespace reduce_accept_language {

// Stores and retrieves the last reduced accept language negotiated for each
// origin, using host content settings.
class ReduceAcceptLanguageService
    : public KeyedService,
      public content::ReduceAcceptLanguageControllerDelegate {
 public:
  ReduceAcceptLanguageService(HostContentSettingsMap* settings_map,
                              PrefService* pref_service,
                              bool is_incognito);

  ReduceAcceptLanguageService(const ReduceAcceptLanguageService&) = delete;
  ReduceAcceptLanguageService& operator=(const ReduceAcceptLanguageService&) =
      delete;

  ~ReduceAcceptLanguageService() override;

  std::optional<std::string> GetReducedLanguage(
      const url::Origin& origin) override;

  std::vector<std::string> GetUserAcceptLanguages() const override;

  void PersistReducedLanguage(const url::Origin& origin,
                              const std::string& language) override;

  void ClearReducedLanguage(const url::Origin& origin) override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Forwards changes to `pref_accept_language_` to `user_accept_languages_`,
  // after formatting them as appropriate.
  void UpdateAcceptLanguage();

  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  const bool is_incognito_;
  StringPrefMember pref_accept_language_;
  // This store user's accept language list listening to changes on language
  // prefs to keep up-to-date.
  std::vector<std::string> user_accept_languages_;
};

}  // namespace reduce_accept_language

#endif  // COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_
