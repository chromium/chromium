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
  using OnTranslationCallback =
      base::OnceCallback<void(const media::SpeechRecognitionResult&)>;

  explicit BabelOrcaCaptionTranslator(
      std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher);
  ~BabelOrcaCaptionTranslator();
  BabelOrcaCaptionTranslator(const BabelOrcaCaptionTranslator&) = delete;
  BabelOrcaCaptionTranslator operator=(const BabelOrcaCaptionTranslator&) =
      delete;

  void Translate(const media::SpeechRecognitionResult& recognition_result,
                 OnTranslationCallback callback,
                 const std::string& source_language,
                 const std::string& target_language);

  // Methods used for setting the current source and target
  // languages in tests.
  void SetDefaultLanguagesForTesting(const std::string& default_source,
                                     const std::string& default_target);
  void UnsetCurrentLanguagesForTesting();

 private:
  // Unwraps and formats output from the translation dispatcher, then passes
  // the result, if successful, to the callback.  Otherwise passes a nullopt
  // to indicate an error.
  void OnTranslationDispatcherCallback(
      OnTranslationCallback callback,
      const std::string& cached_translation,
      const std::string& original_transcription,
      const std::string& source_language,
      const std::string& target_language,
      bool is_final,
      const std::string& result);

  std::optional<std::string> current_source_language_;
  std::optional<std::string> current_target_language_;

  ::captions::TranslationCache translation_cache_;
  std::unique_ptr<BabelOrcaTranslationDipsatcher> translation_dispatcher_;

  base::WeakPtrFactory<BabelOrcaCaptionTranslator> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CAPTION_TRANSLATOR_H_
