// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_

#include <memory>

#include "base/threading/thread.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {

class ConversationControllerProxy;
class LibassistantServiceHost;
class ServiceControllerProxy;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantProxy {
 public:
  using DisplayController = chromeos::libassistant::mojom::DisplayController;

  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  void Initialize(LibassistantServiceHost* host);

  // Returns the controller that manages starting and stopping of the Assistant
  // service.
  ServiceControllerProxy& service_controller();

  // Returns the controller that manages conversations with Libassistant.
  ConversationControllerProxy& conversation_controller_proxy();

  // Returns the controller that manages display related settings.
  DisplayController& display_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

  // Add an observer that will be informed of all speech recognition related
  // updates.
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::SpeechRecognitionObserver> observer);

 private:
  using AudioInputControllerMojom =
      chromeos::libassistant::mojom::AudioInputController;
  using AudioStreamFactoryDelegateMojom =
      chromeos::libassistant::mojom::AudioStreamFactoryDelegate;
  using ConversationControllerMojom =
      chromeos::libassistant::mojom::ConversationController;
  using DisplayControllerMojom =
      chromeos::libassistant::mojom::DisplayController;
  using LibassistantServiceMojom =
      chromeos::libassistant::mojom::LibassistantService;
  using ServiceControllerMojom =
      chromeos::libassistant::mojom::ServiceController;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void LaunchLibassistantService();
  void LaunchLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<LibassistantServiceMojom>);
  void StopLibassistantService();
  void StopLibassistantServiceOnBackgroundThread();

  void BindControllers(LibassistantServiceHost* host);

  // Owned by |AssistantManagerServiceImpl|.
  LibassistantServiceHost* libassistant_service_host_ = nullptr;
  mojo::Remote<LibassistantServiceMojom> libassistant_service_remote_;
  mojo::Remote<DisplayControllerMojom> display_controller_remote_;

  std::unique_ptr<ServiceControllerProxy> service_controller_proxy_;
  std::unique_ptr<ConversationControllerProxy> conversation_controller_proxy_;

  // The thread on which the Libassistant service runs.
  // Warning: must be the last object, so it is destroyed (and flushed) first.
  // This will prevent use-after-free issues where the background thread would
  // access other member variables after they have been destroyed.
  base::Thread background_thread_{"Assistant background thread"};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
