// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

namespace assistant_client {

class ActionModule;
class AssistantManager;
class AssistantManagerDelegate;
class AssistantManagerInternal;
class ConversationStateListener;
class DeviceStateListener;
class FuchsiaApiDelegate;
class PlatformApi;

}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantEventObserver;
class AssistantManagerServiceDelegate;
class CrosDisplayConnection;

class ServiceController {
 public:
  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;

  explicit ServiceController(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  ServiceController(ServiceController&) = delete;
  ServiceController& operator=(ServiceController&) = delete;
  ~ServiceController();

  // Can not be invoked before Start() has finished.
  // Both LibAssistant and Chrome threads may access |display_connection|.
  // |display_connection| is thread safe.
  CrosDisplayConnection* display_connection() {
    DCHECK(IsStarted());
    return display_connection_.get();
  }

  // Can not be invoked before Start() has finished.
  assistant_client::AssistantManager* assistant_manager() {
    DCHECK(IsStarted());
    return assistant_manager_.get();
  }

  // Can not be invoked before Start() has finished.
  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    DCHECK(IsStarted());
    return assistant_manager_internal_;
  }

  // Initialize the |AssistantManager| and all related objects by creating
  // them on a background task and by calling their Start() methods. Will signal
  // the objects exist and can be accessed by calling the |done_callback|.
  //
  // If the |ServiceController| is destroyed before Start()
  // finishes, the created objects will safely be destructed.
  // However, if a new instance of |ServiceController| is immediately
  // created and initialized before the background thread has had any chance to
  // run, it is theoretically possible for 2 instances of |AssistantManager|
  // to exist at the same time. However, this is prevented by the logic in
  // |service.cc|.
  void Start(
      AssistantManagerServiceDelegate* delegate,
      assistant_client::PlatformApi* platform_api,
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
  void Stop();

  // Whether Start() has been called and has finished.
  // Until this is true trying to access any of the getters will fail.
  bool IsStarted() const;

  void UpdateInternalOptions(const std::string& locale,
                             bool spoken_feedback_enabled);

  // Passing in an empty vector will start Libassistant in signed-out mode.
  void SetAuthTokens(const AuthTokens& tokens);

 private:
  enum class State {
    // Start() has been called but the background thread has not finished
    // creating the objects.
    kStarting,
    // All objects have been created and are ready for use.
    kStarted,
    // The objects have not been created and can not be used.
    kStopped,
  };

  void OnAssistantCreated(
      base::OnceClosure done_callback,
      std::unique_ptr<CrosDisplayConnection> display_connection,
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);

  // Used internally for consistency checks.
  State state_ = State::kStopped;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // NOTE: |display_connection_| is used by |assistant_manager_| and must be
  // declared before so it will be destructed after.
  std::unique_ptr<CrosDisplayConnection> display_connection_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;

  base::WeakPtrFactory<ServiceController> weak_factory_;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_H_
