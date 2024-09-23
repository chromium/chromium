// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_service_impl.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/soda_speech_recognizer_impl.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

namespace {

constexpr char kInvalidSpeechRecogntionOptions[] =
    "Invalid SpeechRecognitionOptions provided";

}  // namespace

SpeechRecognitionServiceImpl::SpeechRecognitionServiceImpl(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
    : receiver_(this, std::move(receiver)) {}

SpeechRecognitionServiceImpl::~SpeechRecognitionServiceImpl() = default;

void SpeechRecognitionServiceImpl::BindAudioSourceSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
        context) {
  // TODO(b/249867435): Make this function create mojo::ReportBadMessage because
  // this method shouldn't be called on platforms other than ChromeOS. ChromeOS
  // clients should use CrosSpeechRecognitionService.
  audio_source_speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context) {
  speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::SetSodaPaths(
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name) {
  binary_path_ = binary_path;
  config_paths_ = config_paths;
  primary_language_name_ = primary_language_name;
}

void SpeechRecognitionServiceImpl::SetSodaParams(
    const bool mask_offensive_words) {
  mask_offensive_words_ = mask_offensive_words;
}

void SpeechRecognitionServiceImpl::SetSodaConfigPaths(
    const base::flat_map<std::string, base::FilePath>& config_paths) {
  config_paths_ = config_paths;
  for (Observer& observer : observers_) {
    observer.OnLanguagePackInstalled(config_paths_);
  }
}

void SpeechRecognitionServiceImpl::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  if (CreateRecognizer(std::move(receiver), std::move(client),
                       std::move(options))) {
    std::move(callback).Run(
        SpeechRecognitionRecognizerImpl::IsMultichannelSupported());
  }
}

void SpeechRecognitionServiceImpl::BindWebSpeechRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate,
    media::mojom::SpeechRecognitionOptionsPtr options,
    bool continuous) {
  mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer;
  mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_recognizer_client;

  if (CreateRecognizer(
          speech_recognition_recognizer.InitWithNewPipeAndPassReceiver(),
          speech_recognition_recognizer_client.InitWithNewPipeAndPassRemote(),
          std::move(options))) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SodaSpeechRecognizerImpl>(
            continuous, sample_rate, std::move(speech_recognition_recognizer),
            std::move(speech_recognition_recognizer_client),
            std::move(session_client), std::move(audio_forwarder)),
        std::move(session_receiver));
  }
}

void SpeechRecognitionServiceImpl::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  const bool is_server_based = options->is_server_based;
  // TODO(b/249867435): Remove SpeechRecognitionServiceImpl's implementation of
  // AudioSourceSpeechRecognitionContext should be removed. AudioSourceFetcher
  // is only ever used from ChromeOS and it should be accessed
  // from CrosSpeechRecognitionService.
  if (is_server_based) {
    mojo::ReportBadMessage(kInvalidSpeechRecogntionOptions);
    return;
  }

  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  if (!FilePathsExist()) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return;
  }
  const bool is_multi_channel_supported =
      SpeechRecognitionRecognizerImpl::IsMultichannelSupported();
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(client), std::move(options), binary_path_, config_paths_,
          primary_language_name_, mask_offensive_words_,
          weak_factory_.GetWeakPtr()),
      is_multi_channel_supported, is_server_based);
  std::move(callback).Run(is_multi_channel_supported);
}

void SpeechRecognitionServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SpeechRecognitionServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SpeechRecognitionServiceImpl::FilePathsExist() {
  if (!base::PathExists(binary_path_))
    return false;

  for (const auto& config : config_paths_) {
    if (!base::PathExists(config.second))
      return false;
  }

  return true;
}

bool SpeechRecognitionServiceImpl::CreateRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options) {
  // This is currently only used by Live Caption and server side
  // speech recognition is not available to it.
  if (options->is_server_based ||
      options->recognizer_client_type !=
          media::mojom::RecognizerClientType::kLiveCaption) {
    mojo::ReportBadMessage(kInvalidSpeechRecogntionOptions);
    return false;
  }

  // Destroy the speech recognition service if the SODA files haven't been
  // downloaded yet.
  if (!FilePathsExist()) {
    speech_recognition_contexts_.Clear();
    receiver_.reset();
    return false;
  }

  SpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client), std::move(options), binary_path_,
      config_paths_, primary_language_name_, mask_offensive_words_,
      weak_factory_.GetWeakPtr());

  return true;
}

}  // namespace speech
