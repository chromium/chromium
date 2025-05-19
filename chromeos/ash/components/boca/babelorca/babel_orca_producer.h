// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_PRODUCER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_PRODUCER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::babelorca {

class CaptionController;
class TachyonRequestDataProvider;
class TokenManager;
class TranscriptSenderRateLimiter;

// Class to control captions handling behavior in producer mode.
class BabelOrcaProducer : public BabelOrcaController,
                          public BabelOrcaSpeechRecognizer::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Public for testing.
  //
  // LINT.IfChange(SendingStoppedReason)
  enum class SendingStoppedReason {
    kSessionEnded = 0,
    kSessionCaptionTurnedOff = 1,
    kTachyonSendMessagesError = 2,
    kMaxValue = kTachyonSendMessagesError,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:BabelOrcaSendingStoppedReason)

  static std::unique_ptr<BabelOrcaController> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
      std::unique_ptr<CaptionController> caption_controller,
      std::unique_ptr<BabelOrcaCaptionTranslator> translator,
      TokenManager* oauth_token_manager,
      TachyonRequestDataProvider* request_data_provider);

  BabelOrcaProducer(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
      std::unique_ptr<CaptionController> caption_controller,
      std::unique_ptr<babelorca::TachyonAuthedClient> authed_client,
      TachyonRequestDataProvider* request_data_provider,
      std::unique_ptr<BabelOrcaCaptionTranslator> translator);

  ~BabelOrcaProducer() override;

  // BabelOrcaController:
  void OnSessionStarted() override;
  void OnSessionEnded() override;
  void OnSessionCaptionConfigUpdated(bool session_captions_enabled,
                                     bool translations_enabled) override;
  void OnLocalCaptionConfigUpdated(bool local_captions_enabled) override;
  bool IsProducer() override;

  // BabelOrcaSpeechRecognizer::Observer:
  void OnTranscriptionResult(const media::SpeechRecognitionResult& result,
                             const std::string& source_language) override;
  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event) override;

 private:
  void InitSending(bool signed_in);

  void OnSendFailed();

  void StopRecognition();

  void DispatchToBubble(const media::SpeechRecognitionResult& result);

  SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const std::unique_ptr<CaptionController> caption_controller_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const std::unique_ptr<BabelOrcaCaptionTranslator> translator_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<babelorca::TachyonAuthedClient> authed_client_;
  const raw_ptr<TachyonRequestDataProvider> request_data_provider_;

  std::unique_ptr<TranscriptSenderRateLimiter> rate_limited_sender_;

  bool local_captions_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool session_captions_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  bool in_session_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::WeakPtrFactory<BabelOrcaProducer> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_PRODUCER_H_
