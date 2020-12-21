// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_

#include <mutex>
#include <string>

#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Fake implementation of the Mojom |ServiceController|.
// This implementation will inform the registered |StateObserver| instances of
// any state change, just like the real implementation.
class FakeServiceController : public libassistant::mojom::ServiceController {
 public:
  using State = libassistant::mojom::ServiceState;
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  FakeServiceController();
  FakeServiceController(FakeServiceController&) = delete;
  FakeServiceController& operator=(FakeServiceController&) = delete;
  ~FakeServiceController() override;

  // Puts the service in the given state. Will inform all observers of the state
  // change.
  void SetState(State new_state);
  State state() const { return state_; }

  // Returns the Libassistant config that was passed to the last Start() call.
  std::string libassistant_config() { return libassistant_config_; }

  void Bind(mojo::PendingReceiver<libassistant::mojom::ServiceController>
                pending_receiver);
  void Unbind();

  void SetInitializeCallback(InitializeCallback callback);

  // Call this to block any call to |Start|. The observers will not be invoked
  // as long as the start call is blocked. Unblock these calls using
  // |UnblockStartCalls|. This is not enabled by default, so unless you call
  // |BlockStartCalls| any |Start| call will simply finish immediately.
  void BlockStartCalls();
  void UnblockStartCalls();

  // mojom::ServiceController implementation:
  void Start(const std::string& libassistant_config) override;
  void Stop() override;
  void AddAndFireStateObserver(
      mojo::PendingRemote<libassistant::mojom::StateObserver> pending_observer)
      override;

 private:
  // Mutex taken in |Start| to allow the calls to block if |BlockStartCalls| was
  // called.
  std::mutex start_mutex_;

  // Config passed to LibAssistant when it was started.
  std::string libassistant_config_;

  InitializeCallback initialize_callback_;

  State state_ = State::kStopped;
  mojo::Receiver<libassistant::mojom::ServiceController> receiver_;
  mojo::RemoteSet<libassistant::mojom::StateObserver> state_observers_;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_
