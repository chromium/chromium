// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_producer.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender_impl.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender_rate_limiter.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::babelorca {
namespace {

constexpr char kSendingStoppedReasonUma[] =
    "Ash.Boca.Babelorca.SendingStoppedReason";

}  // namespace

// static
std::unique_ptr<BabelOrcaController> BabelOrcaProducer::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
    std::unique_ptr<CaptionController> caption_controller,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator,
    TokenManager* oauth_token_manager,
    TachyonRequestDataProvider* request_data_provider) {
  return std::make_unique<BabelOrcaProducer>(
      url_loader_factory, std::move(speech_recognizer),
      std::move(caption_controller),
      std::make_unique<TachyonAuthedClientImpl>(
          std::make_unique<babelorca::TachyonClientImpl>(url_loader_factory),
          oauth_token_manager),
      request_data_provider, std::move(translator));
}

BabelOrcaProducer::BabelOrcaProducer(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
    std::unique_ptr<CaptionController> caption_controller,
    std::unique_ptr<TachyonAuthedClient> authed_client,
    TachyonRequestDataProvider* request_data_provider,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator)
    : speech_recognizer_(std::move(speech_recognizer)),
      caption_controller_(std::move(caption_controller)),
      translator_(std::move(translator)),
      authed_client_(std::move(authed_client)),
      request_data_provider_(request_data_provider) {
  if (!features::IsBocaTranslateToggleEnabled()) {
    // Translation is always enabled for producer.
    caption_controller_->SetLiveTranslateEnabled(true);
  }
}

BabelOrcaProducer::~BabelOrcaProducer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "[BabelOrca] stop recognition in dtor";
  StopRecognition();
}

void BabelOrcaProducer::OnSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "[BabelOrca] session started";
  in_session_ = true;
}

void BabelOrcaProducer::OnSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_session_ && session_captions_enabled_) {
    base::UmaHistogramEnumeration(kSendingStoppedReasonUma,
                                  SendingStoppedReason::kSessionEnded);
  }
  VLOG(1) << "[BabelOrca] session ended";
  in_session_ = false;
  session_captions_enabled_ = false;
  rate_limited_sender_.reset();
  if (!local_captions_enabled_) {
    VLOG(1) << "[BabelOrca] local captions disabled, stop recognition";
    StopRecognition();
  }
}

void BabelOrcaProducer::OnSessionCaptionConfigUpdated(
    bool session_captions_enabled,
    bool translations_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_session_ && session_captions_enabled_ && !session_captions_enabled) {
    base::UmaHistogramEnumeration(
        kSendingStoppedReasonUma,
        SendingStoppedReason::kSessionCaptionTurnedOff);
  }
  if (!in_session_) {
    LOG(ERROR)
        << "[BabelOrca] Session caption config event called out of session.";
    return;
  }
  // Producer currently does not process translations. If the captions enabled
  // state hasn't changed, return fast.
  if (session_captions_enabled_ == session_captions_enabled) {
    VLOG(1) << "[BabelOrca] session captions already "
            << session_captions_enabled_;
    return;
  }

  session_captions_enabled_ = session_captions_enabled;
  if (!session_captions_enabled_ && !local_captions_enabled_) {
    VLOG(1) << "[BabelOrca] all disabled, stop recognition";
    StopRecognition();
    return;
  }
  if (!session_captions_enabled_) {
    VLOG(1) << "[BabelOrca] session captions disabled, reset sender";
    rate_limited_sender_.reset();
    return;
  }

  if (request_data_provider_->tachyon_token().has_value()) {
    VLOG(1) << "[BabelOrca] already has tachyon token, init sending";
    InitSending(/*signed_in=*/true);
    return;
  }
  VLOG(1) << "[BabelOrca] signin to tachyon";
  request_data_provider_->SigninToTachyonAndRespond(base::BindOnce(
      &BabelOrcaProducer::InitSending, weak_ptr_factory_.GetWeakPtr()));
}

void BabelOrcaProducer::OnLocalCaptionConfigUpdated(
    bool local_captions_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_captions_enabled_ = local_captions_enabled;
  if (!session_captions_enabled_ && !local_captions_enabled_) {
    VLOG(1) << "[BabelOrca] all disabled, stop recognition";
    StopRecognition();
    return;
  }
  if (!local_captions_enabled_) {
    // Close the bubble.
    VLOG(1) << "[BabelOrca] local captions disabled, close bubble";
    caption_controller_->StopLiveCaption();
    return;
  } else {
    // Ensure bubble creation.
    VLOG(1) << "[BabelOrca] local captions enabled, ensure bubble creation";
    caption_controller_->StartLiveCaption();
  }
  // If session captions enabled and sender is initialized, this means that
  // recognition already started and observation is set.
  if (session_captions_enabled_ && rate_limited_sender_) {
    VLOG(1) << "[BabelOrca] recognition already started by session captions";
    return;
  }

  VLOG(1)
      << "[BabelOrca] observe and start speech recognition for local captions";
  speech_recognizer_->RemoveObserver(this);
  speech_recognizer_->AddObserver(this);
  speech_recognizer_->Start();
}

bool BabelOrcaProducer::IsProducer() {
  return true;
}

void BabelOrcaProducer::InitSending(bool signed_in) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!signed_in) {
    // TODO(crbug.com//373692250): report error.
    LOG(ERROR) << "[BabelOrca] Failed to signin to Tachyon";
    return;
  }
  // Check if session caption was disabled before request is completed or
  // session ended. Session ended will set `session_captions_enabled_` to false.
  if (!session_captions_enabled_) {
    VLOG(1) << "[BabelOrca] session caption disabled, do not init sending";
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
    VLOG(1) << "[BabelOrca] recognition already started by local captions";
    return;
  }
  VLOG(1) << "[BabelOrca] observe and start speech recognition for session "
             "captions";
  speech_recognizer_->RemoveObserver(this);
  speech_recognizer_->AddObserver(this);
  speech_recognizer_->Start();
}

void BabelOrcaProducer::OnTranscriptionResult(
    const media::SpeechRecognitionResult& result,
    const std::string& source_language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!features::IsBocaTranslateToggleEnabled() ||
      caption_controller_->IsTranslateAllowedAndEnabled()) {
    translator_->Translate(
        result,
        base::BindOnce(&BabelOrcaProducer::DispatchToBubble,
                       weak_ptr_factory_.GetWeakPtr()),
        source_language,
        caption_controller_->GetLiveTranslateTargetLanguageCode());
  } else {
    DispatchToBubble(result);
  }

  // `session_captions_enabled_` can be enabled but `rate_limited_sender_` is
  // not initialized because signin is not complete.
  if (!session_captions_enabled_ || !rate_limited_sender_) {
    return;
  }
  rate_limited_sender_->Send(result, source_language);
}

void BabelOrcaProducer::OnLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  caption_controller_->OnLanguageIdentificationEvent(event);
}

void BabelOrcaProducer::OnSendFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/373692250): report error.
  base::UmaHistogramEnumeration(
      kSendingStoppedReasonUma,
      SendingStoppedReason::kTachyonSendMessagesError);
  LOG(ERROR) << "[BabelOrca] Transcript send permanently failed";
  session_captions_enabled_ = false;
  rate_limited_sender_.reset();
  if (!local_captions_enabled_) {
    VLOG(1)
        << "[BabelOrca] stop recognition on send failure and local captions "
           "disabled";
    StopRecognition();
  }
}

void BabelOrcaProducer::StopRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  speech_recognizer_->Stop();
  caption_controller_->StopLiveCaption();
  // This should be a no-op if not currently observing.
  speech_recognizer_->RemoveObserver(this);
  rate_limited_sender_.reset();
}

void BabelOrcaProducer::DispatchToBubble(
    const media::SpeechRecognitionResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_captions_enabled_) {
    return;
  }
  bool dispatch_success = caption_controller_->DispatchTranscription(result);
  // TODO(crbug.com/373692250): add dispatch attempts error limit and report
  // failure.
  VLOG_IF(1, !dispatch_success)
      << "Caption bubble transcription dispatch failed";
}

}  // namespace ash::babelorca
