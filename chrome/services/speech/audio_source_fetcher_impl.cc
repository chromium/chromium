// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/audio_source_fetcher_impl.h"

#include "build/build_config.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_mixer.h"
#include "media/base/limits.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/cpp/device_factory.h"

namespace speech {

AudioSourceFetcherImpl::AudioSourceFetcherImpl(
    std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer)
    : speech_recognition_recognizer_(std::move(recognition_recognizer)),
      is_started_(false) {}

AudioSourceFetcherImpl::~AudioSourceFetcherImpl() {
  Stop();
}

void AudioSourceFetcherImpl::Create(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
    std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<AudioSourceFetcherImpl>(
                                  std::move(recognition_recognizer)),
                              std::move(receiver));
}

void AudioSourceFetcherImpl::Start(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    const ::media::AudioParameters& audio_parameters) {
  // If we've already started fetching audio from this device with these params,
  // return early. Otherwise start over and reset.
  if (is_started_) {
    if (device_id == device_id_ && audio_parameters.Equals(audio_parameters_)) {
      LOG(ERROR)
          << "AudioSourceFetcher was already running, and was asked to restart "
             "with the same device ID and audio parameters. Doing nothing.";
      return;
    } else {
      Stop();
    }
  }

  device_id_ = device_id;
  audio_parameters_ = audio_parameters;
  audio_capturer_source_ =
      audio::CreateInputDevice(std::move(stream_factory), device_id_,
                               audio::DeadStreamDetection::kEnabled);
  DCHECK(audio_capturer_source_);

  // TODO(crbug.com/1185978): Check implementation / sandbox policy on Mac and
  // Windows.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  is_started_ = true;
  // Initialize the AudioCapturerSource with |this| as the CaptureCallback,
  // get the parameters for the device ID, then start audio capture.
  send_audio_callback_ = media::BindToCurrentLoop(base::BindRepeating(
      &AudioSourceFetcherImpl::SendAudioToSpeechRecognitionService,
      weak_factory_.GetWeakPtr()));
  GetAudioCapturerSource()->Initialize(audio_parameters_, this);
  GetAudioCapturerSource()->Start();
#endif
}

void AudioSourceFetcherImpl::Stop() {
  if (GetAudioCapturerSource()) {
    GetAudioCapturerSource()->Stop();
    audio_capturer_source_.reset();
  }
  send_audio_callback_.Reset();
  is_started_ = false;
}

void AudioSourceFetcherImpl::Capture(const media::AudioBus* audio_source,
                                     base::TimeTicks audio_capture_time,
                                     double volume,
                                     bool key_pressed) {
  // Called on a worker thread created by the AudioCapturerSource.
  // (See |media::AudioDeviceThread|).
  auto audio_bus =
      media::AudioBus::Create(audio_source->channels(), audio_source->frames());
  DCHECK(audio_bus);
  audio_source->CopyTo(audio_bus.get());
  // Send the audio callback from the main thread.
  send_audio_callback_.Run(ConvertToAudioDataS16(
      std::move(audio_bus), audio_parameters_.sample_rate(),
      audio_parameters_.channel_layout(),
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported()));
}

void AudioSourceFetcherImpl::OnCaptureError(const std::string& message) {
  speech_recognition_recognizer_->OnSpeechRecognitionError();
}

void AudioSourceFetcherImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
      std::move(buffer));
}

media::AudioCapturerSource* AudioSourceFetcherImpl::GetAudioCapturerSource() {
  return audio_capturer_source_for_tests_ ? audio_capturer_source_for_tests_
                                          : audio_capturer_source_.get();
}

}  // namespace speech
