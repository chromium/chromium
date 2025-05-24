// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONSUMER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/babelorca/transcript_receiver.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::babelorca {

class CaptionController;
class TachyonRequestDataProvider;
class TachyonResponse;
class TokenManager;

class BabelOrcaConsumer : public BabelOrcaController {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Public for testing.
  //
  // LINT.IfChange(ReceivingStoppedReason)
  enum class ReceivingStoppedReason {
    kSessionEnded = 0,
    kSessionCaptionTurnedOff = 1,
    kLocalCaptionTurnedOff = 2,
    kTachyonSigninError = 3,
    kJoinGroupError = 4,
    kTachyonReceiveMessagesError = 5,
    kMaxValue = kTachyonReceiveMessagesError,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:BabelOrcaReceivingStoppedReason)

  static std::unique_ptr<BabelOrcaController> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      GaiaId gaia_id,
      std::string school_tools_base_url,
      std::unique_ptr<CaptionController> caption_controller,
      std::unique_ptr<BabelOrcaCaptionTranslator> translator,
      PrefService* pref_service,
      TokenManager* tachyon_oauth_token_manager,
      TachyonRequestDataProvider* tachyon_request_data_provider);

  BabelOrcaConsumer(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const GaiaId& gaia_id,
      std::string school_tools_base_url,
      std::unique_ptr<CaptionController> caption_controller,
      TokenManager* tachyon_oauth_token_manager,
      TachyonRequestDataProvider* tachyon_request_data_provider,
      TranscriptReceiver::StreamingClientGetter streaming_client_getter,
      std::unique_ptr<BabelOrcaCaptionTranslator> translator,
      PrefService* pref_service);

  ~BabelOrcaConsumer() override;

  // BabelOrcaController:
  void OnSessionStarted() override;
  void OnSessionEnded() override;
  void OnSessionCaptionConfigUpdated(bool session_captions_enabled,
                                     bool translations_enabled) override;
  void OnLocalCaptionConfigUpdated(bool local_captions_enabled) override;
  bool IsProducer() override;

 private:
  void OnTranslationCallback(
      const std::optional<media::SpeechRecognitionResult>& result);
  void DispatchTranscription(const media::SpeechRecognitionResult& result);

  void StartReceiving();

  void OnSignedIn(bool success);

  void JoinSessionTachyonGroup();

  void OnJoinGroupResponse(TachyonResponse response);

  void OnTranscriptReceived(media::SpeechRecognitionResult transcript,
                            std::string language);

  void OnReceivingFailed();

  void StopReceiving();

  void Reset();

  bool IsReceivingCaptions();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const GaiaId gaia_id_;
  const std::string school_tools_base_url_;
  const std::unique_ptr<CaptionController> caption_controller_;
  const raw_ptr<TokenManager> tachyon_oauth_token_manager_;
  const raw_ptr<TachyonRequestDataProvider> tachyon_request_data_provider_;
  TranscriptReceiver::StreamingClientGetter streaming_client_getter_;
  std::string join_group_url_;
  std::unique_ptr<TokenManager> join_group_token_manager_;
  std::unique_ptr<TachyonAuthedClient> join_group_authed_client_;
  const raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  const std::unique_ptr<BabelOrcaCaptionTranslator> translator_;

  std::unique_ptr<TranscriptReceiver> transcript_receiver_;

  bool signed_in_ = false;
  bool joined_group_ = false;
  bool local_captions_enabled_ = false;
  bool session_captions_enabled_ = false;
  bool in_session_ = false;

  base::WeakPtrFactory<BabelOrcaConsumer> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONSUMER_H_
