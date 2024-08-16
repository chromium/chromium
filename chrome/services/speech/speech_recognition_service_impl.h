// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_
#define CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace speech {

// Implements the SpeechRecognitionService with SODA on-device speech
// recognition.
class SpeechRecognitionServiceImpl
    : public media::mojom::SpeechRecognitionService,
      public media::mojom::AudioSourceSpeechRecognitionContext,
      public media::mojom::SpeechRecognitionContext {
 public:
  // Observer of the speech recognition service.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a new language pack is installed.
    virtual void OnLanguagePackInstalled(
        base::flat_map<std::string, base::FilePath> config_paths) = 0;
  };

  explicit SpeechRecognitionServiceImpl(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver);

  SpeechRecognitionServiceImpl(const SpeechRecognitionServiceImpl&) = delete;
  SpeechRecognitionServiceImpl& operator=(const SpeechRecognitionServiceImpl&) =
      delete;

  ~SpeechRecognitionServiceImpl() override;

  // media::mojom::SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context)
      override;
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          context) override;
  void SetSodaPaths(
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name) override;
  void SetSodaParams(const bool mask_offensive_words) override;
  void SetSodaConfigPaths(
      const base::flat_map<std::string, base::FilePath>& config_paths) override;

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;
  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override;

  // media::mojom::AudioSourceSpeechRecognitionContext:
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindAudioSourceFetcherCallback callback) override;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

 protected:
  // Returns whether the binary and config paths exist.
  bool FilePathsExist();

  // Returns whether the recognizer was successfully created.
  bool CreateRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options);

  mojo::Receiver<media::mojom::SpeechRecognitionService> receiver_;

  // The sets of receivers used to receive messages from the clients.
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  mojo::ReceiverSet<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_speech_recognition_contexts_;

  base::FilePath binary_path_ = base::FilePath();
  base::flat_map<std::string, base::FilePath> config_paths_;
  std::string primary_language_name_;
  bool mask_offensive_words_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<SpeechRecognitionServiceImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_
