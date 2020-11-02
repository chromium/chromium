// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_CONTROLLER_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

namespace assistant_client {

class PlatformApi;
class ActionModule;
class FuchsiaApiDelegate;
class AssistantManager;
class AssistantManagerInternal;

}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerServiceDelegate;
class CrosDisplayConnection;
class AssistantManagerServiceImpl;

class AssistantManagerController {
 public:
  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;

  AssistantManagerController();

  AssistantManagerController(AssistantManagerController&) = delete;
  AssistantManagerController& operator=(AssistantManagerController&) = delete;
  ~AssistantManagerController();

  // Can not be invoked before initialization has finished.
  // Both LibAssistant and Chrome threads may access |display_connection|.
  // |display_connection| is thread safe.
  CrosDisplayConnection* display_connection() {
    DCHECK(IsInitialized());
    return display_connection_.get();
  }

  // Can not be invoked before initialization has finished.
  assistant_client::AssistantManager* assistant_manager() {
    DCHECK(IsInitialized());
    return assistant_manager_.get();
  }

  // Can not be invoked before initialization has finished.
  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    DCHECK(IsInitialized());
    return assistant_manager_internal_;
  }

  // Initialize the |AssistantManager| by creating the objects (on a background
  // task) and by calling their Start() methods. Will signal the objects exist
  // and can be accessed by calling the |done_callback|.
  //
  // If the |AssistantManagerController| is destroyed before Initialize()
  // finishes, the created objects will safely be destructed.
  // However, if any of the passed in objects (|service|, |delegate|,
  // |platform_api| and so on) are destroyed, the caller *must* destroy
  // |background_task_runner| first or invalid memory might be accessed.
  // Also, if a new instance of |AssistantManagerController| is immediately
  // created and initialized before the background thread has had any chance to
  // run, it is theoretically possible for 2 instances of |AssistantManager|
  // to exist at the same time. However, this is prevented by the logic in
  // |service.cc|.
  void Initialize(
      AssistantManagerServiceImpl* service,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
      AssistantManagerServiceDelegate* delegate,
      assistant_client::PlatformApi* platform_api,
      assistant_client::ActionModule* action_module,
      assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
      const std::string& libassistant_config,
      const std::string& locale,
      const std::string& locale_override,
      bool spoken_feedback_enabled,
      const AuthTokens& auth_tokens,
      base::OnceClosure done_callback);

  // Whether Initialize() has been called and has finished.
  // Until this is true trying to access any of the getters will fail.
  bool IsInitialized() const;

  void UpdateInternalOptions(const std::string& locale,
                             bool spoken_feedback_enabled);

  // Passing in an empty vector will start Libassistant in signed-out mode.
  void SetAuthTokens(const AuthTokens& tokens);

 private:
  void OnAssistantCreated(
      base::OnceClosure done_callback,
      std::unique_ptr<CrosDisplayConnection> display_connection,
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);

  // NOTE: |display_connection_| is used by |assistant_manager_| and must be
  // declared before so it will be destructed after.
  std::unique_ptr<CrosDisplayConnection> display_connection_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;

  base::WeakPtrFactory<AssistantManagerController> weak_factory_;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_CONTROLLER_H_
