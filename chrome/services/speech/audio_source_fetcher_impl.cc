// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/audio_source_fetcher_impl.h"

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/cpp/device_factory.h"

namespace speech {

AudioSourceFetcherImpl::AudioSourceFetcherImpl(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer)
    : speech_recognition_recognizer_(std::move(recognition_recognizer)) {
  std::string device_id = media::AudioDeviceDescription::kDefaultDeviceId;
  audio_capturer_source_ =
      audio::CreateInputDevice(std::move(stream_factory), device_id,
                               audio::DeadStreamDetection::kEnabled);
  DCHECK(audio_capturer_source_);
}

AudioSourceFetcherImpl::~AudioSourceFetcherImpl() {
  Stop();
}

void AudioSourceFetcherImpl::Create(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    std::unique_ptr<SpeechRecognitionRecognizerImpl> recognition_recognizer) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AudioSourceFetcherImpl>(
          std::move(stream_factory), std::move(recognition_recognizer)),
      std::move(receiver));
}

void AudioSourceFetcherImpl::Stop() {
  if (audio_capturer_source_) {
    audio_capturer_source_->Stop();
    audio_capturer_source_ = nullptr;
  }
  speech_recognition_recognizer_.reset();
}

}  // namespace speech
