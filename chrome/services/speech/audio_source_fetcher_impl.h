// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
#define CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "media/base/audio_capturer_source.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

class SpeechRecognitionRecognizerImpl;

// Class to get microphone audio data and send it to a
// SpeechRecognitionRecognizerImpl for transcription. Runs on the IO thread in
// the Browser process in Chrome OS and in the Speech Recognition Service
// utility process on Chrome or web speech fallback.
class AudioSourceFetcherImpl
    : public media::mojom::AudioSourceFetcher,
      public media::AudioCapturerSource::CaptureCallback,
      public media::AudioDataS16Converter {
 public:
  AudioSourceFetcherImpl(
      std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer);
  ~AudioSourceFetcherImpl() override;
  AudioSourceFetcherImpl(const AudioSourceFetcherImpl&) = delete;
  AudioSourceFetcherImpl& operator=(const AudioSourceFetcherImpl&) = delete;

  static void Create(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
      std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer);

  // media::mojom::AudioSourceFetcher:
  void Start(
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      const std::string& device_id,
      const ::media::AudioParameters& audio_parameters) override;
  void Stop() override;

  // media::AudioCapturerSource::CaptureCallback:
  void OnCaptureStarted() final {}
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) final;
  void OnCaptureError(const std::string& message) final;
  void OnCaptureMuted(bool is_muted) final {}

  void set_audio_capturer_source_for_tests(
      media::AudioCapturerSource* audio_capturer_source_for_tests) {
    audio_capturer_source_for_tests_ = audio_capturer_source_for_tests;
  }

 private:
  using SendAudioToSpeechRecognitionServiceCallback =
      base::RepeatingCallback<void(media::mojom::AudioDataS16Ptr audio_data)>;

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer);

  media::AudioCapturerSource* GetAudioCapturerSource();

  // Sends audio to the speech recognition recognizer.
  SendAudioToSpeechRecognitionServiceCallback send_audio_callback_;

  // Audio capturer source for microphone recording.
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  media::AudioCapturerSource* audio_capturer_source_for_tests_ = nullptr;

  // Audio parameters will be used when recording audio.
  media::AudioParameters audio_parameters_;

  // Device ID used to record audio.
  std::string device_id_;

  // Owned SpeechRecognitionRecognizerImpl was constructed by the
  // SpeechRecognitionService as appropriate for the platform.
  std::unique_ptr<SpeechRecognitionRecognizerImpl>
      speech_recognition_recognizer_;

  // Whether audio capture is started.
  bool is_started_;

  base::WeakPtrFactory<AudioSourceFetcherImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
