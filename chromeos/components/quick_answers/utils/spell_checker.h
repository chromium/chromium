// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/utils/spell_check_language.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace quick_answers {

// Utility class for spell check.
class SpellChecker : public QuickAnswersStateObserver {
 public:
  using CheckSpellingCallback =
      base::OnceCallback<void(bool, const std::string&)>;
  using SpellCheckLanguageIterator =
      std::vector<std::unique_ptr<SpellCheckLanguage>>::iterator;

  explicit SpellChecker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  SpellChecker(const SpellChecker&) = delete;
  SpellChecker& operator=(const SpellChecker&) = delete;

  ~SpellChecker() override;

  // Check spelling of the given word, run |callback| with true if the word is
  // spelled correctly. Virtual for testing.
  virtual void CheckSpelling(const std::string& word,
                             CheckSpellingCallback callback);

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool enabled) override;
  void OnConsentStatusUpdated(prefs::ConsentStatus status) override;
  void OnApplicationLocaleReady(const std::string& locale) override;
  void OnPreferredLanguagesChanged(
      const std::string& preferred_languages) override;
  void OnEligibilityChanged(bool eligible) override;
  void OnPrefsInitialized() override;

  base::WeakPtr<SpellChecker> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::vector<std::unique_ptr<SpellCheckLanguage>>&
  GetSpellcheckLanguagesForTesting() {
    return spellcheck_languages_;
  }

 private:
  // Check feature eligibility and correspondingly update languages list. If
  // |should_recreate_languages_list| is false, languages list will not be
  // updated as long as feature eligibility stay unchanged.
  // Called when the Quick answers states are updated.
  void CheckEligibilityAndUpdateLanguages(bool should_recreate_languages_list);

  // Collect spell check results from the language indicated by |iterator|. Run
  // |callback| with true if the word is found in the current language,
  // otherwise continue to check the next language.
  void CollectResults(const std::string& word,
                      CheckSpellingCallback callback,
                      SpellCheckLanguageIterator iterator,
                      const std::string& language,
                      int languages_list_version,
                      bool is_correct);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // List of spell check languages.
  std::vector<std::unique_ptr<SpellCheckLanguage>> spellcheck_languages_;

  // Version of the languages list to invalidate pending calls if the languages
  // has ben updated.
  int languages_list_version_ = 0;

  base::ScopedObservation<QuickAnswersState, QuickAnswersStateObserver>
      quick_answers_state_observation_{this};

  base::WeakPtrFactory<SpellChecker> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_
