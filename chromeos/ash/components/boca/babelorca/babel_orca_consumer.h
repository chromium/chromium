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
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/babelorca/transcript_receiver.h"

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
  static std::unique_ptr<BabelOrcaController> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      std::string gaia_id,
      std::unique_ptr<CaptionController> caption_controller,
      TokenManager* tachyon_oauth_token_manager,
      TachyonRequestDataProvider* tachyon_request_data_provider);

  BabelOrcaConsumer(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const std::string& gaia_id,
      std::unique_ptr<CaptionController> caption_controller,
      TokenManager* tachyon_oauth_token_manager,
      TachyonRequestDataProvider* tachyon_request_data_provider,
      TranscriptReceiver::StreamingClientGetter streaming_client_getter);

  ~BabelOrcaConsumer() override;

  // BabelOrcaController:
  void OnSessionStarted() override;
  void OnSessionEnded() override;
  void OnSessionCaptionConfigUpdated(bool session_captions_enabled) override;
  void OnLocalCaptionConfigUpdated(bool local_captions_enabled) override;

 private:
  void StartReceiving();

  void OnSignedIn(bool success);

  void JoinSessionTachyonGroup();

  void OnJoinGroupResponse(TachyonResponse response);

  void OnTrasncriptReceived(media::SpeechRecognitionResult transcript,
                            std::string language);

  void OnReceivingFailed();

  void StopReceiving();

  void Reset();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const std::string gaia_id_;
  const std::unique_ptr<CaptionController> caption_controller_;
  const raw_ptr<TokenManager> tachyon_oauth_token_manager_;
  const raw_ptr<TachyonRequestDataProvider> tachyon_request_data_provider_;
  TranscriptReceiver::StreamingClientGetter streaming_client_getter_;
  std::string join_group_url_;
  std::unique_ptr<TokenManager> join_group_token_manager_;
  std::unique_ptr<TachyonAuthedClient> join_group_authed_client_;

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
