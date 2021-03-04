// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
#define CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "media/base/audio_capturer_source.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

class SpeechRecognitionRecognizerImpl;

// Class to get microphone audio data and send it to a
// SpeechRecognitionRecognizerImpl for transcription. Runs in Browser process in
// Chrome OS and Speech Recognition Service on Chrome or web speech fallback.
// TODO(crbug.com/1173135): Override from
// media::AudioCapturerSource::CaptureCallback to capture audio.
class AudioSourceFetcherImpl : public media::mojom::AudioSourceFetcher {
 public:
  AudioSourceFetcherImpl(
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer);
  ~AudioSourceFetcherImpl() override;
  AudioSourceFetcherImpl(const AudioSourceFetcherImpl&) = delete;
  AudioSourceFetcherImpl& operator=(const AudioSourceFetcherImpl&) = delete;

  static void Create(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer);

  // media::mojom::AudioSourceFetcher:
  void Stop() override;

 private:
  // Audio capturerer source for microphone recording.
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;

  // Owned SpeechRecognitionRecognizerImpl was constructed by the
  // SpeechRecognitionService as appropriate for the platform.
  std::unique_ptr<SpeechRecognitionRecognizerImpl>
      speech_recognition_recognizer_;

  base::WeakPtrFactory<AudioSourceFetcherImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
