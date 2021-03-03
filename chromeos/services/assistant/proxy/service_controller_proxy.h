// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant_client {

class AssistantManagerDelegate;
class ConversationStateListener;

}  // namespace assistant_client

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace assistant {

class LibassistantServiceHost;

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
class ServiceControllerProxy {
 public:
  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;
  using BootupConfig = libassistant::mojom::BootupConfig;
  using BootupConfigPtr = libassistant::mojom::BootupConfigPtr;

  ServiceControllerProxy(
      LibassistantServiceHost* host,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
          client);

  ServiceControllerProxy(ServiceControllerProxy&) = delete;
  ServiceControllerProxy& operator=(ServiceControllerProxy&) = delete;
  ~ServiceControllerProxy();

  // Initialize the |AssistantManager| and all related objects.
  //
  // Start() can only be called when the service is stopped.
  void Start(
      assistant_client::AssistantManagerDelegate* assistant_manager_delegate,
      assistant_client::ConversationStateListener* conversation_state_listener,
      BootupConfigPtr bootup_config,
      const AuthTokens& auth_tokens);
  // Stop and destroy the |AssistantManager| and all related objects.
  void Stop();
  void ResetAllDataAndStop();

  void SetSpokenFeedbackEnabled(bool value);

  // Passing in an empty vector will start Libassistant in signed-out mode.
  void SetAuthTokens(const AuthTokens& tokens);

  void AddAndFireStateObserver(
      ::mojo::PendingRemote<libassistant::mojom::StateObserver> observer);

 private:
  mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory();

  // Owned by |AssistantManagerServiceImpl| which (indirectly) also owns us.
  LibassistantServiceHost* const host_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  mojo::Remote<chromeos::libassistant::mojom::ServiceController>
      service_controller_remote_;

  base::WeakPtrFactory<ServiceControllerProxy> weak_factory_{this};
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
