// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

class GaiaId;

namespace ash::babelorca {
class BabelOrcaController;
}  // namespace ash::babelorca

namespace boca {
class UserIdentity;
}  // namespace boca

namespace captions {
class CaptionBubbleContext;
}  // namespace captions

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash::boca {

// Manager for BabelOrca observing BOCA session events and doing captions
// related actions accordingly.
class BabelOrcaManager : public BocaSessionManager::Observer,
                         public babelorca::TachyonRequestDataProvider {
 public:
  using ControllerFactory =
      base::OnceCallback<std::unique_ptr<babelorca::BabelOrcaController>(
          babelorca::TokenManager*,
          babelorca::TachyonRequestDataProvider*)>;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static std::unique_ptr<BabelOrcaManager> CreateAsProducer(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
      std::unique_ptr<babelorca::BabelOrcaSpeechRecognizer> speech_recognizer,
      std::unique_ptr<babelorca::BabelOrcaCaptionTranslator> translator,
      base::RepeatingClosure on_local_caption_closed_cb,
      PrefService* pref_service,
      const std::string& application_locale,
      const std::string& caption_language);

  static std::unique_ptr<BabelOrcaManager> CreateAsConsumer(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
      const GaiaId& gaia_id,
      std::string school_tools_url_base,
      std::unique_ptr<babelorca::BabelOrcaCaptionTranslator> translator,
      base::RepeatingClosure on_local_caption_closed_cb,
      PrefService* pref_service,
      const std::string& application_locale,
      const std::string& caption_language);

  BabelOrcaManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ControllerFactory controller_factory);

  BabelOrcaManager(const BabelOrcaManager&) = delete;
  BabelOrcaManager& operator=(const BabelOrcaManager&) = delete;

  ~BabelOrcaManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnSessionCaptionConfigUpdated(
      const std::string& group_name,
      const ::boca::CaptionsConfig& config,
      const std::string& tachyon_group_id) override;
  void OnLocalCaptionConfigUpdated(
      const ::boca::CaptionsConfig& config) override;
  void OnLocalCaptionClosed() override;
  void OnSessionCaptionClosed(bool is_error) override;

  bool IsCaptioningAvailable();

  // babelorca::TachyonRequestDataProvider
  void SigninToTachyonAndRespond(
      base::OnceCallback<void(bool)> on_response_cb) override;
  std::optional<std::string> session_id() const override;
  std::optional<std::string> tachyon_token() const override;
  std::optional<std::string> group_id() const override;
  std::optional<std::string> sender_email() const override;

 private:
  raw_ptr<PrefService> pref_service_;
  babelorca::TokenManagerImpl token_manager_;
  babelorca::TachyonAuthedClientImpl authed_client_;
  babelorca::TachyonRegistrar registrar_;

  std::optional<std::string> session_id_;
  std::optional<std::string> group_id_;
  std::optional<std::string> sender_email_;

  std::unique_ptr<babelorca::BabelOrcaController> babel_orca_controller_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
