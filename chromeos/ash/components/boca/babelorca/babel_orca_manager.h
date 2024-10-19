// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "components/live_caption/translation_dispatcher.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

// Manager for BabelOrca observing BOCA session events and doing captions
// related actions accordingly.
class BabelOrcaManager : public BocaSessionManager::Observer,
                         public babelorca::TachyonRequestDataProvider {
 public:
  BabelOrcaManager(
      std::unique_ptr<captions::TranslationDispatcher> translation_dispatcher,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  BabelOrcaManager(const BabelOrcaManager&) = delete;
  BabelOrcaManager& operator=(const BabelOrcaManager&) = delete;
  ~BabelOrcaManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;

  bool IsCaptioningAvailable();

  // babelorca::TachyonRequestDataProvider
  void SigninToTachyonAndRespond(
      base::OnceCallback<void(bool)> on_response_cb) override;
  std::optional<std::string> session_id() const override;
  std::optional<std::string> tachyon_token() const override;
  std::optional<std::string> group_id() const override;
  std::optional<std::string> sender_email() const override;

 private:
  std::unique_ptr<captions::TranslationDispatcher> translation_dispatcher_;
  const std::string client_uuid_;
  babelorca::TokenManagerImpl token_manager_;
  babelorca::TachyonAuthedClientImpl authed_client_;
  babelorca::TachyonRegistrar registrar_;

  std::optional<std::string> session_id_;
  std::optional<std::string> group_id_;
  std::optional<std::string> sender_email_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
