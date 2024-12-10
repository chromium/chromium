// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_producer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender_impl.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender_rate_limiter.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::babelorca {

// static
std::unique_ptr<BabelOrcaController> BabelOrcaProducer::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
    std::unique_ptr<LiveCaptionControllerWrapper> caption_controller_wrapper,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator,
    PrefService* pref_service,
    TokenManager* oauth_token_manager,
    TachyonRequestDataProvider* request_data_provider) {
  return std::make_unique<BabelOrcaProducer>(
      url_loader_factory, std::move(speech_recognizer),
      std::move(caption_controller_wrapper),
      std::make_unique<TachyonAuthedClientImpl>(
          std::make_unique<babelorca::TachyonClientImpl>(url_loader_factory),
          oauth_token_manager),
      request_data_provider, std::move(translator), pref_service);
}

BabelOrcaProducer::BabelOrcaProducer(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
    std::unique_ptr<LiveCaptionControllerWrapper> caption_controller_wrapper,
    std::unique_ptr<TachyonAuthedClient> authed_client,
    TachyonRequestDataProvider* request_data_provider,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator,
    PrefService* pref_service)
    : speech_recognizer_(std::move(speech_recognizer)),
      caption_controller_wrapper_(std::move(caption_controller_wrapper)),
      translator_(std::move(translator)),
      pref_service_(pref_service),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      authed_client_(std::move(authed_client)),
      request_data_provider_(request_data_provider) {
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kLiveTranslateTargetLanguageCode,
      base::BindRepeating(&BabelOrcaProducer::OnTranslationPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // TODO(377696975) re-factor translator.
  translator_->InitTranslationAndSetCallback(
      base::BindRepeating(&BabelOrcaProducer::OnTranslationCallback,
                          weak_ptr_factory_.GetWeakPtr()),
      pref_service_->GetString(prefs::kUserMicrophoneCaptionLanguageCode),
      pref_service_->GetString(prefs::kLiveTranslateTargetLanguageCode));
}

BabelOrcaProducer::~BabelOrcaProducer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopRecognition();
}

void BabelOrcaProducer::OnSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  in_session_ = true;
}

void BabelOrcaProducer::OnSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  in_session_ = false;
  session_captions_enabled_ = false;
  rate_limited_sender_.reset();
  if (!local_captions_enabled_) {
    StopRecognition();
  }
}

void BabelOrcaProducer::OnSessionCaptionConfigUpdated(
    bool session_captions_enabled,
    bool translations_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!in_session_) {
    LOG(ERROR) << "Session caption config event called out of session.";
    return;
  }
  // Producer currently does not process translations. If the captions enabled
  // state hasn't changed, return fast.
  if (session_captions_enabled_ == session_captions_enabled) {
    return;
  }

  session_captions_enabled_ = session_captions_enabled;
  if (!session_captions_enabled_ && !local_captions_enabled_) {
    StopRecognition();
    return;
  }
  if (!session_captions_enabled_) {
    rate_limited_sender_.reset();
    return;
  }

  if (request_data_provider_->tachyon_token().has_value()) {
    InitSending(/*signed_in=*/true);
    return;
  }
  request_data_provider_->SigninToTachyonAndRespond(base::BindOnce(
      &BabelOrcaProducer::InitSending, weak_ptr_factory_.GetWeakPtr()));
}

void BabelOrcaProducer::OnLocalCaptionConfigUpdated(
    bool local_captions_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_captions_enabled_ = local_captions_enabled;
  caption_controller_wrapper_->ToggleLiveCaptionForBabelOrca(
      local_captions_enabled_);
  if (!session_captions_enabled_ && !local_captions_enabled_) {
    StopRecognition();
    return;
  }
  if (!local_captions_enabled_) {
    // Close the bubble.
    caption_controller_wrapper_->OnAudioStreamEnd();
    return;
  }
  // If session captions enabled and sender is initialized, this means that
  // recognition already started and observation is set.
  if (session_captions_enabled_ && rate_limited_sender_) {
    return;
  }

  speech_recognizer_->ObserveTranscriptionResult(
      base::BindRepeating(&BabelOrcaProducer::OnTranscriptionResult,
                          weak_ptr_factory_.GetWeakPtr()));
  speech_recognizer_->Start();
}

void BabelOrcaProducer::InitSending(bool signed_in) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!signed_in) {
    // TODO(crbug.com//373692250): report error.
    LOG(ERROR) << "Failed to signin to Tachyon";
    return;
  }
  // Check if session caption was disabled before request is completed or
  // session ended. Session ended will set `session_captions_enabled_` to false.
  if (!session_captions_enabled_) {
    return;
  }
  auto sender = std::make_unique<TranscriptSenderImpl>(
      authed_client_.get(), request_data_provider_, base::Time::Now(),
      TranscriptSenderImpl::Options(),
      base::BindOnce(&BabelOrcaProducer::OnSendFailed, base::Unretained(this)));
  rate_limited_sender_ =
      std::make_unique<TranscriptSenderRateLimiter>(std::move(sender));
  // If local captions enabled, this means that recognition already started and
  // observation is set.
  if (local_captions_enabled_) {
    return;
  }
  speech_recognizer_->ObserveTranscriptionResult(
      base::BindRepeating(&BabelOrcaProducer::OnTranscriptionResult,
                          weak_ptr_factory_.GetWeakPtr()));
  speech_recognizer_->Start();
}

void BabelOrcaProducer::OnTranscriptionResult(
    const media::SpeechRecognitionResult& result,
    const std::string& source_language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If  the source and target languages for translation are the
  // same then the translator will simply forward the result
  // to the callback function which is set during construction and
  // when the translation target language changes with the translator's
  // `InitTranslationAndSetCallback` function.
  //
  // The callback we set here will then dispatch the result to the
  // live caption bubble.
  //
  // This is confusing, but the next CL downstream of this one simplifies
  // the translator's API and makes it clear what is happening here.
  //
  // TODO(377696975) re-factor translator.
  translator_->Translate(result);

  // `session_captions_enabled_` can be enabled but `rate_limited_sender_` is
  // not initialized because signin is not complete.
  if (!session_captions_enabled_ || !rate_limited_sender_) {
    return;
  }
  rate_limited_sender_->Send(result, source_language);
}

void BabelOrcaProducer::OnSendFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/373692250): report error.
  LOG(ERROR) << "Transcript send permanently failed";
  session_captions_enabled_ = false;
  rate_limited_sender_.reset();
  if (!local_captions_enabled_) {
    StopRecognition();
  }
}

void BabelOrcaProducer::StopRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  speech_recognizer_->Stop();
  caption_controller_wrapper_->OnAudioStreamEnd();
  // This should be a no-op if not currently observing.
  speech_recognizer_->RemoveTranscriptionResultObservation();
  rate_limited_sender_.reset();
}

void BabelOrcaProducer::OnTranslationPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  translator_->InitTranslationAndSetCallback(
      base::BindRepeating(&BabelOrcaProducer::OnTranslationCallback,
                          weak_ptr_factory_.GetWeakPtr()),
      pref_service_->GetString(prefs::kUserMicrophoneCaptionLanguageCode),
      pref_service_->GetString(prefs::kLiveTranslateTargetLanguageCode));
}

void BabelOrcaProducer::OnTranslationCallback(
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (!result) {
    VLOG(1) << "Failed to recieve translation";
    return;
  }

  DispatchToBubble(result.value());
}

void BabelOrcaProducer::DispatchToBubble(
    const media::SpeechRecognitionResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_captions_enabled_) {
    return;
  }

  bool dispatch_success =
      caption_controller_wrapper_->DispatchTranscription(result);
  if (!dispatch_success) {
    // Restart captions in case the bubble was closed.
    caption_controller_wrapper_->RestartCaptions();
    dispatch_success =
        caption_controller_wrapper_->DispatchTranscription(result);
  }

  // TODO(crbug.com/373692250): add dispatch attempts error limit and report
  // failure.
  VLOG_IF(1, !dispatch_success)
      << "Caption bubble transcription dispatch failed";
}

}  // namespace ash::babelorca
