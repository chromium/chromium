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
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::babelorca {

class BabelOrcaSpeechRecognizer;
class LiveCaptionControllerWrapper;
class TachyonRequestDataProvider;
class TokenManager;
class TranscriptSenderRateLimiter;

// Class to control captions handling behavior in producer mode.
class BabelOrcaProducer : public BabelOrcaController {
 public:
  static std::unique_ptr<BabelOrcaController> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
      std::unique_ptr<LiveCaptionControllerWrapper> caption_controller_wrapper,
      TokenManager* oauth_token_manager,
      TachyonRequestDataProvider* request_data_provider);

  BabelOrcaProducer(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer,
      std::unique_ptr<LiveCaptionControllerWrapper> caption_controller_wrapper,
      std::unique_ptr<babelorca::TachyonAuthedClient> authed_client,
      TachyonRequestDataProvider* request_data_provider);

  ~BabelOrcaProducer() override;

  // BabelOrcaController:
  void OnSessionStarted() override;
  void OnSessionEnded() override;
  void OnSessionCaptionConfigUpdated(bool session_captions_enabled,
                                     bool translations_enabled) override;
  void OnLocalCaptionConfigUpdated(bool local_captions_enabled) override;

 private:
  void InitSending(bool signed_in);

  void OnTranscriptionResult(const media::SpeechRecognitionResult& result,
                             const std::string& source_language);

  void OnSendFailed();

  void StopRecognition();

  SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<BabelOrcaSpeechRecognizer> speech_recognizer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const std::unique_ptr<LiveCaptionControllerWrapper>
      caption_controller_wrapper_ GUARDED_BY_CONTEXT(sequence_checker_);
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
