// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/service_controller_proxy.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom-forward.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace assistant {

// TODO(b/171748795): Most of the work that is done here right now (especially
// the work related to starting Libassistant) should be moved to the mojom
// service.

namespace {

using libassistant::mojom::ServiceState;

struct StartArguments {
  StartArguments() = default;
  StartArguments(StartArguments&&) = default;
  StartArguments& operator=(StartArguments&&) = default;
  ~StartArguments() = default;

  assistant_client::AssistantManagerDelegate* assistant_manager_delegate;
  assistant_client::ConversationStateListener* conversation_state_listener;
};

// TODO(b/171748795): This should all be migrated to the mojom service, which
// should be responsible for the complete creation of the Libassistant
// objects.
// Note: this method will be called from the mojom (background) thread.
void InitializeAssistantManager(
    StartArguments arguments,
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_internal->SetAssistantManagerDelegate(
      arguments.assistant_manager_delegate);
  assistant_manager->AddConversationStateListener(
      arguments.conversation_state_listener);
}

std::vector<libassistant::mojom::AuthenticationTokenPtr>
ToMojomAuthenticationTokens(const ServiceControllerProxy::AuthTokens& tokens) {
  std::vector<libassistant::mojom::AuthenticationTokenPtr> result;

  for (const std::pair<std::string, std::string>& token : tokens) {
    result.emplace_back(libassistant::mojom::AuthenticationTokenPtr(
        base::in_place, /*gaia_id=*/token.first,
        /*access_token=*/token.second));
  }

  return result;
}

}  // namespace

ServiceControllerProxy::ServiceControllerProxy(
    LibassistantServiceHost* host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
        client)
    : host_(host),
      url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))),
      service_controller_remote_(std::move(client)) {}

ServiceControllerProxy::~ServiceControllerProxy() = default;

void ServiceControllerProxy::Start(
    assistant_client::AssistantManagerDelegate* assistant_manager_delegate,
    assistant_client::ConversationStateListener* conversation_state_listener,
    BootupConfigPtr bootup_config,
    const AuthTokens& auth_tokens) {
  // We need to initialize the |AssistantManager| once it's created and before
  // it's started, so we register a callback to do just that.
  StartArguments arguments;
  arguments.assistant_manager_delegate = assistant_manager_delegate;
  arguments.conversation_state_listener = conversation_state_listener;
  host_->SetInitializeCallback(
      base::BindOnce(InitializeAssistantManager, std::move(arguments)));

  // The mojom service will create the |AssistantManager|.
  service_controller_remote_->Initialize(std::move(bootup_config),
                                         BindURLLoaderFactory());
  SetAuthTokens(auth_tokens);
  service_controller_remote_->Start();
}

void ServiceControllerProxy::Stop() {
  service_controller_remote_->Stop();
}

void ServiceControllerProxy::ResetAllDataAndStop() {
  service_controller_remote_->ResetAllDataAndStop();
}

void ServiceControllerProxy::SetSpokenFeedbackEnabled(bool value) {
  service_controller_remote_->SetSpokenFeedbackEnabled(value);
}

void ServiceControllerProxy::SetAuthTokens(const AuthTokens& tokens) {
  service_controller_remote_->SetAuthenticationTokens(
      ToMojomAuthenticationTokens(tokens));
}

void ServiceControllerProxy::AddAndFireStateObserver(
    ::mojo::PendingRemote<libassistant::mojom::StateObserver> observer) {
  service_controller_remote_->AddAndFireStateObserver(std::move(observer));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ServiceControllerProxy::BindURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  url_loader_factory_->Clone(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace assistant
}  // namespace chromeos
