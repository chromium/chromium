// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_service_impl.h"

#include "base/files/file_util.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/base/media_switches.h"

namespace speech {

SpeechRecognitionServiceImpl::SpeechRecognitionServiceImpl(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
    : receiver_(this, std::move(receiver)) {}

SpeechRecognitionServiceImpl::~SpeechRecognitionServiceImpl() = default;

void SpeechRecognitionServiceImpl::BindContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context) {
  speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::SetUrlLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = mojo::Remote<network::mojom::URLLoaderFactory>(
      std::move(url_loader_factory));
  url_loader_factory_.set_disconnect_handler(
      base::BindOnce(&SpeechRecognitionServiceImpl::DisconnectHandler,
                     base::Unretained(this)));
}

void SpeechRecognitionServiceImpl::SetSodaPath(
    const base::FilePath& binary_path,
    const base::FilePath& config_path) {
  binary_path_ = binary_path;
  config_path_ = config_path;
}

void SpeechRecognitionServiceImpl::BindSpeechRecognitionServiceClient(
    mojo::PendingRemote<media::mojom::SpeechRecognitionServiceClient> client) {
  client_ = mojo::Remote<media::mojom::SpeechRecognitionServiceClient>(
      std::move(client));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
SpeechRecognitionServiceImpl::GetUrlLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory_remote;
  url_loader_factory_->Clone(
      pending_factory_remote.InitWithNewPipeAndPassReceiver());

  return pending_factory_remote;
}

void SpeechRecognitionServiceImpl::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) &&
      (!base::PathExists(binary_path_) || !base::PathExists(config_path_))) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return;
  }

  SpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client), weak_factory_.GetWeakPtr(),
      binary_path_, config_path_);
  std::move(callback).Run(
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void SpeechRecognitionServiceImpl::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  // TODO(crbug.com/1173135): Pass enable_soda as an options parameter instead
  // of using media::kUseSodaForLiveCaption.
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) &&
      (!base::PathExists(binary_path_) || !base::PathExists(config_path_))) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return;
  }
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(client), weak_factory_.GetWeakPtr(), binary_path_,
          config_path_));
  std::move(callback).Run(
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void SpeechRecognitionServiceImpl::DisconnectHandler() {
  if (client_.is_bound())
    client_->OnNetworkServiceDisconnect();
}

}  // namespace speech
