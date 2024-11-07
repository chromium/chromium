// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_consumer.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_producer.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper_impl.h"
#include "chromeos/ash/components/boca/babelorca/oauth_token_fetcher.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"
#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {

// static
std::unique_ptr<BabelOrcaManager> BabelOrcaManager::CreateAsProducer(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    captions::LiveCaptionController* live_caption_controller,
    std::unique_ptr<captions::CaptionBubbleContext> caption_bubble_context,
    std::unique_ptr<babelorca::BabelOrcaSpeechRecognizer> speech_recognizer) {
  ControllerFactory controller_factory = base::BindOnce(
      babelorca::BabelOrcaProducer::Create, url_loader_factory,
      std::move(speech_recognizer),
      std::make_unique<babelorca::LiveCaptionControllerWrapperImpl>(
          live_caption_controller, std::move(caption_bubble_context)));
  return std::make_unique<BabelOrcaManager>(
      identity_manager, url_loader_factory, std::move(controller_factory));
}

// static
std::unique_ptr<BabelOrcaManager> BabelOrcaManager::CreateAsConsumer(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<babelorca::CaptionController> caption_controller,
    const std::string& gaia_id,
    std::unique_ptr<babelorca::BabelOrcaCaptionTranslator> translator,
    PrefService* pref_service) {
  ControllerFactory controller_factory =
      base::BindOnce(babelorca::BabelOrcaConsumer::Create, url_loader_factory,
                     identity_manager, gaia_id, std::move(caption_controller),
                     std::move(translator), pref_service);
  return std::make_unique<BabelOrcaManager>(
      identity_manager, url_loader_factory, std::move(controller_factory));
}

BabelOrcaManager::BabelOrcaManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ControllerFactory controller_factory)
    : client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      token_manager_(
          std::make_unique<babelorca::OAuthTokenFetcher>(identity_manager)),
      authed_client_(
          std::make_unique<babelorca::TachyonClientImpl>(url_loader_factory),
          &token_manager_),
      registrar_(&authed_client_),
      babel_orca_controller_(
          std::move(controller_factory).Run(&token_manager_, this)) {}

BabelOrcaManager::~BabelOrcaManager() = default;

void BabelOrcaManager::OnSessionStarted(const std::string& session_id,
                                        const ::boca::UserIdentity& producer) {
  session_id_ = session_id;
  sender_email_ = producer.email();
  // `babel_orca_controller_` can be null in tests.
  if (!babel_orca_controller_) {
    return;
  }
  babel_orca_controller_->OnSessionStarted();
}

void BabelOrcaManager::OnSessionEnded(const std::string& session_id) {
  if (babel_orca_controller_) {
    babel_orca_controller_->OnSessionEnded();
  }
  session_id_.reset();
  sender_email_.reset();
  group_id_.reset();
  registrar_.ResetToken();
}

void BabelOrcaManager::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config,
    const std::string& tachyon_group_id) {
  group_id_ = tachyon_group_id;
  if (!babel_orca_controller_) {
    return;
  }
  babel_orca_controller_->OnSessionCaptionConfigUpdated(
      config.captions_enabled(), config.translations_enabled());
}

void BabelOrcaManager::OnLocalCaptionConfigUpdated(
    const ::boca::CaptionsConfig& config) {
  if (!babel_orca_controller_) {
    return;
  }
  babel_orca_controller_->OnLocalCaptionConfigUpdated(
      config.captions_enabled());
}

bool BabelOrcaManager::IsCaptioningAvailable() {
  // TODO(b/361086008): Implement IsCaptioningAvailable();
  return true;
}

void BabelOrcaManager::SigninToTachyonAndRespond(
    base::OnceCallback<void(bool)> on_response_cb) {
  registrar_.Register(client_uuid_, base::BindOnce(std::move(on_response_cb)));
}

std::optional<std::string> BabelOrcaManager::session_id() const {
  return session_id_;
}

std::optional<std::string> BabelOrcaManager::tachyon_token() const {
  return registrar_.GetTachyonToken();
}

std::optional<std::string> BabelOrcaManager::group_id() const {
  return group_id_;
}

std::optional<std::string> BabelOrcaManager::sender_email() const {
  return sender_email_;
}

}  // namespace ash::boca
