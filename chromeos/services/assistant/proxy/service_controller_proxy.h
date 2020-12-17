// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant_client {

class ActionModule;
class AssistantManager;
class AssistantManagerDelegate;
class AssistantManagerInternal;
class ConversationStateListener;
class DeviceStateListener;
class FuchsiaApiDelegate;

}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantEventObserver;
class CrosDisplayConnection;
class LibassistantServiceHost;

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
class ServiceControllerProxy : private libassistant::mojom::StateObserver {
 public:
  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;

  ServiceControllerProxy(
      LibassistantServiceHost* host,
      mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
          client);

  ServiceControllerProxy(ServiceControllerProxy&) = delete;
  ServiceControllerProxy& operator=(ServiceControllerProxy&) = delete;
  ~ServiceControllerProxy() override;

  // Can not be invoked before Start() has finished.
  CrosDisplayConnection* display_connection() {
    DCHECK(display_connection_);
    return display_connection_.get();
  }

  // Initialize the |AssistantManager| and all related objects.
  // Will signal the objects exist and can be accessed by calling the
  // |done_callback|.
  //
  // Start() can only be called when the service is stopped.
  void Start(
      assistant_client::ActionModule* action_module,
      assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
      assistant_client::AssistantManagerDelegate* assistant_manager_delegate,
      assistant_client::ConversationStateListener* conversation_state_listener,
      assistant_client::DeviceStateListener* device_state_listener,
      AssistantEventObserver* event_observer,
      const std::string& libassistant_config,
      const std::string& locale,
      const std::string& locale_override,
      bool spoken_feedback_enabled,
      const AuthTokens& auth_tokens,
      base::OnceClosure done_callback);
  // Stop and destroy the |AssistantManager| and all related objects.
  // Stop() can not be called if the service is starting.
  void Stop();

  // Whether Start() has been called and has finished.
  // Until this is true trying to access any of the getters will fail.
  bool IsStarted() const;

  void UpdateInternalOptions(const std::string& locale,
                             bool spoken_feedback_enabled);

  // Passing in an empty vector will start Libassistant in signed-out mode.
  void SetAuthTokens(const AuthTokens& tokens);

 private:
  // TODO(jeroendh): Once the entire start procedure has been moved to the
  // Libassistant mojom service we will no longer need the |kStarting| state,
  // which means we can probably delete this enum and simply rely on the
  // |libassistant::mojom::ServiceState| enum.
  enum class State {
    // Start() has been called but the background thread has not finished
    // creating the objects.
    kStarting,
    // All objects have been created and are ready for use.
    kStarted,
    // The objects have not been created and can not be used.
    kStopped,
  };

  // Can not be invoked before Start() has finished.
  assistant_client::AssistantManager* assistant_manager();

  // Can not be invoked before Start() has finished.
  assistant_client::AssistantManagerInternal* assistant_manager_internal();

  void FinishCreatingAssistant();

  // libassistant::mojom::StateObserver implementation:
  void OnStateChanged(libassistant::mojom::ServiceState new_state) override;

  void OnAssistantStarted(base::OnceClosure done_callback);

  // Used internally for consistency checks.
  State state_ = State::kStopped;

  // Owned by |AssistantManagerServiceImpl| which (indirectly) also owns us.
  LibassistantServiceHost* const host_;

  mojo::Remote<chromeos::libassistant::mojom::ServiceController>
      service_controller_remote_;
  mojo::Receiver<chromeos::libassistant::mojom::StateObserver>
      state_observer_receiver_;

  // Callback passed to Start(). Will be invoked once the Libassistant service
  // has started.
  base::Optional<base::OnceClosure> on_start_done_callback_;

  std::unique_ptr<CrosDisplayConnection> display_connection_;
  // Populated when we're starting but not started yet, so after Start() has
  // been called but before the mojom service signalled it has started.
  std::unique_ptr<CrosDisplayConnection> pending_display_connection_;

  base::WeakPtrFactory<ServiceControllerProxy> weak_factory_{this};
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
