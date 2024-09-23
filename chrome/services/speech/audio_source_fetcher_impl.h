// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
#define CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/services/speech/audio_source_consumer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/converting_audio_fifo.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

class SpeechRecognitionRecognizerImpl;

// Class to get device audio data and send it to a
// SpeechRecognitionRecognizerImpl for transcription. Runs on the IO thread in
// the Browser process in Chrome OS and in the Speech Recognition Service
// utility process on Chrome or web speech fallback.
class AudioSourceFetcherImpl
    : public media::mojom::AudioSourceFetcher,
      public media::AudioCapturerSource::CaptureCallback,
      public media::AudioDataS16Converter,
      public media::mojom::AudioLog {
 public:
  AudioSourceFetcherImpl(
      std::unique_ptr<AudioSourceConsumer> recognition_recognizer,
      bool is_multi_channel_supported,
      bool is_server_based);
  ~AudioSourceFetcherImpl() override;
  AudioSourceFetcherImpl(const AudioSourceFetcherImpl&) = delete;
  AudioSourceFetcherImpl& operator=(const AudioSourceFetcherImpl&) = delete;

  static void Create(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
      std::unique_ptr<AudioSourceConsumer> recognition_recognizer,
      bool is_multi_channel_supported,
      bool is_server_based);

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
               const media::AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) final;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) final;
  void OnCaptureMuted(bool is_muted) final {}
  // media::mojom::AudioLog
  void OnCreated(const media::AudioParameters& params,
                 const std::string& device_id) override;
  void OnStarted() override;
  void OnStopped() override;
  void OnClosed() override;
  void OnError() override;
  void OnSetVolume(double volume) override;
  void OnLogMessage(const std::string& message) override;
  void OnProcessingStateChanged(const std::string& message) override;

  // The output callback for ConvertingAudioFifo.
  void OnAudioFinishedConvert(const media::AudioBus* output_bus);

  void set_audio_capturer_source_for_tests(
      media::AudioCapturerSource* audio_capturer_source_for_tests) {
    audio_capturer_source_for_tests_ = audio_capturer_source_for_tests;
  }

 private:
  using SendAudioToSpeechRecognitionServiceCallback =
      base::RepeatingCallback<void(media::mojom::AudioDataS16Ptr audio_data)>;
  using SendAudioToResampleCallback = base::RepeatingCallback<void(
      std::unique_ptr<media::AudioBus> audio_data)>;

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer);

  void SendAudioToResample(std::unique_ptr<media::AudioBus> audio_data);

  void SendAudioEndToSpeechRecognitionService();

  void SendError();

  media::AudioCapturerSource* GetAudioCapturerSource();

  void DrainConverterOutput();

  // Sends audio to the speech recognition recognizer.
  SendAudioToSpeechRecognitionServiceCallback send_audio_callback_;

  // Audio capturer source for microphone recording.
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  raw_ptr<media::AudioCapturerSource> audio_capturer_source_for_tests_ =
      nullptr;

  // Audio parameters will be used when recording audio.
  media::AudioParameters audio_parameters_;

  // Device ID used to record audio.
  std::string device_id_;

  // Owned AudioSourceConsumer
  std::unique_ptr<AudioSourceConsumer> audio_consumer_;

  // Whether audio capture is started.
  bool is_started_;

  mojo::Receiver<media::mojom::AudioLog> audio_log_receiver_{this};

  // Used to resample the audio when using server based speech recognition. Null
  // when using SODA.
  std::unique_ptr<media::ConvertingAudioFifo> converter_;

  // The output params for resampling for the server based speech recognition.
  std::optional<media::AudioParameters> server_based_recognition_params_ =
      std::nullopt;
  bool is_multi_channel_supported_;
  bool is_server_based_;

  base::TimeDelta audio_length_ = base::Seconds(0);

  // A callback to push audio data into `converter_`.
  SendAudioToResampleCallback resample_callback_;

  // Callback bound to correct thread to send errors to `audio_consumer_`.
  base::RepeatingClosure send_error_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioSourceFetcherImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_AUDIO_SOURCE_FETCHER_IMPL_H_
