// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
class AssistantManagerServiceDelegate;
class CrosPlatformApi;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class ConversationController;
class DisplayController;
class PlatformApi;
class ServiceController;

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) LibassistantService
    : public mojom::LibassistantService {
 public:
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  explicit LibassistantService(
      mojo::PendingReceiver<mojom::LibassistantService> receiver,
      chromeos::assistant::CrosPlatformApi* platform_api,
      assistant::AssistantManagerServiceDelegate* delegate);
  LibassistantService(LibassistantService&) = delete;
  LibassistantService& operator=(LibassistantService&) = delete;
  ~LibassistantService() override;

  void SetInitializeCallback(InitializeCallback callback);

 private:
  ServiceController& service_controller() { return *service_controller_; }

  // mojom::LibassistantService implementation:
  void Bind(
      mojo::PendingReceiver<mojom::AudioInputController> audio_input_controller,
      mojo::PendingRemote<mojom::AudioStreamFactoryDelegate>
          audio_stream_factory_delegate,
      mojo::PendingReceiver<mojom::ConversationController>
          conversation_controller,
      mojo::PendingReceiver<mojom::DisplayController> display_controller,
      mojo::PendingReceiver<mojom::ServiceController> service_controller)
      override;
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<mojom::SpeechRecognitionObserver> observer) override;

  mojo::Receiver<mojom::LibassistantService> receiver_;

  mojo::RemoteSet<mojom::SpeechRecognitionObserver>
      speech_recognition_observers_;

  std::unique_ptr<PlatformApi> platform_api_;
  std::unique_ptr<DisplayController> display_controller_;
  std::unique_ptr<ServiceController> service_controller_;
  std::unique_ptr<ConversationController> conversation_controller_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
