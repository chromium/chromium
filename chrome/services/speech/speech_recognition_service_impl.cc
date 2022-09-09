// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_service_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace speech {

SpeechRecognitionServiceImpl::SpeechRecognitionServiceImpl(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
    : receiver_(this, std::move(receiver)) {}

SpeechRecognitionServiceImpl::~SpeechRecognitionServiceImpl() = default;

void SpeechRecognitionServiceImpl::BindAudioSourceSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
        context) {
  audio_source_speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context) {
  speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::SetSodaPath(
    const base::FilePath& binary_path,
    const base::FilePath& config_path) {
  binary_path_ = binary_path;
  config_path_ = config_path;
}

void SpeechRecognitionServiceImpl::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  if (!base::PathExists(binary_path_) || !base::PathExists(config_path_)) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return;
  }

  SpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client), weak_factory_.GetWeakPtr(),
      std::move(options), binary_path_, config_path_);
  std::move(callback).Run(
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void SpeechRecognitionServiceImpl::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  if (!base::PathExists(binary_path_) || !base::PathExists(config_path_)) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return;
  }
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(client), weak_factory_.GetWeakPtr(), std::move(options),
          binary_path_, config_path_));
  std::move(callback).Run(
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

}  // namespace speech
