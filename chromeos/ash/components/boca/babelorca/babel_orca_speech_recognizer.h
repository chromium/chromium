// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_H_

#include <string>

#include "base/observer_list_types.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

// Interface that allows control and observation of speech recognition for
// BabelOrca.
class BabelOrcaSpeechRecognizer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTranscriptionResult(
        const media::SpeechRecognitionResult& result,
        const std::string& source_language) = 0;
    virtual void OnLanguageIdentificationEvent(
        const media::mojom::LanguageIdentificationEventPtr& event) = 0;
  };

  BabelOrcaSpeechRecognizer(const BabelOrcaSpeechRecognizer&) = delete;
  BabelOrcaSpeechRecognizer& operator=(const BabelOrcaSpeechRecognizer&) =
      delete;

  virtual ~BabelOrcaSpeechRecognizer() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

 protected:
  BabelOrcaSpeechRecognizer() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_SPEECH_RECOGNIZER_H_
