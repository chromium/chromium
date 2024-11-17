// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_TRANSLATOR_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_TRANSLATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class BabelOrcaCaptionTranslator {
 public:
  using OnTranslationCallback = base::RepeatingCallback<void(
      const std::optional<media::SpeechRecognitionResult>&)>;

  explicit BabelOrcaCaptionTranslator(
      std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher);
  ~BabelOrcaCaptionTranslator();
  BabelOrcaCaptionTranslator(const BabelOrcaCaptionTranslator&) = delete;
  BabelOrcaCaptionTranslator operator=(const BabelOrcaCaptionTranslator&) =
      delete;

  // Called before calls to translate.  This sets the source and target language
  // as well as the callback that will trigger once translation has completed.
  void InitTranslationAndSetCallback(OnTranslationCallback callback,
                                     const std::string& source_language,
                                     const std::string& target_language);

  // Called to cleanup the callback if translations are no longer needed.
  void UnsetOnTranslationCallback() { callback_.Reset(); }

  // Translates results contents if `recognition_result` is present and the
  // OnTranslationCallback is set.  this method does nothing if
  //`InitTranslationAndSetCallback` hasn't been called.
  void Translate(
      const std::optional<media::SpeechRecognitionResult>& recognition_result);

 private:
  // Utility for ensuring we're only comparing the language components of
  // locales.
  static std::string GetLanguageComponentFromLocale(const std::string& locale);

  // Unwraps and formats output from the translation dispatcher, then passes
  // the result, if successful, to the callback.  Otherwise passes a nullopt
  // to indicate an error.
  void OnTranslationDispatcherCallback(
      const std::string& cached_translation,
      const std::string& original_transcription,
      const std::string& source_language,
      const std::string& target_language,
      bool is_final,
      const std::string& result);

  bool IsNonIdeographicSourceOrIdeographicTarget();
  bool AreLanguagesTheSame();

  std::string source_language_;
  std::string target_language_;

  OnTranslationCallback callback_;
  ::captions::TranslationCache translation_cache_;
  std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher_;

  base::WeakPtrFactory<BabelOrcaCaptionTranslator> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_TRANSLATOR_H_
