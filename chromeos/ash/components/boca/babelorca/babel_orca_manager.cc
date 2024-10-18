// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "chromeos/ash/components/boca/babelorca/oauth_token_fetcher.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"
#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {

BabelOrcaManager::BabelOrcaManager(
    std::unique_ptr<captions::TranslationDispatcher> translation_dispatcher,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : translation_dispatcher_(std::move(translation_dispatcher)),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      token_manager_(
          std::make_unique<babelorca::OAuthTokenFetcher>(identity_manager)),
      authed_client_(
          std::make_unique<babelorca::TachyonClientImpl>(url_loader_factory),
          &token_manager_),
      registrar_(&authed_client_) {}

BabelOrcaManager::~BabelOrcaManager() = default;

void BabelOrcaManager::OnSessionStarted(const std::string& session_id,
                                        const ::boca::UserIdentity& producer) {
  session_id_ = session_id;
  sender_email_ = producer.email();
}

void BabelOrcaManager::OnSessionEnded(const std::string& session_id) {
  session_id_.reset();
  sender_email_.reset();
  group_id_.reset();
  registrar_.ResetToken();
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
