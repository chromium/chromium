// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
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
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class AudioInputController;
class ConversationController;
class ConversationStateListenerImpl;
class DisplayController;
class MediaController;
class PlatformApi;
class ServiceController;
class SpeakerIdEnrollmentController;

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) LibassistantService
    : public mojom::LibassistantService {
 public:
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  explicit LibassistantService(
      mojo::PendingReceiver<mojom::LibassistantService> receiver,
      assistant::AssistantManagerServiceDelegate* delegate);
  LibassistantService(LibassistantService&) = delete;
  LibassistantService& operator=(LibassistantService&) = delete;
  ~LibassistantService() override;

  void SetInitializeCallback(InitializeCallback callback);

  // mojom::LibassistantService implementation:
  void Bind(
      mojo::PendingReceiver<mojom::AudioInputController> audio_input_controller,
      mojo::PendingReceiver<mojom::ConversationController>
          conversation_controller,
      mojo::PendingReceiver<mojom::DisplayController> display_controller,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingReceiver<mojom::ServiceController> service_controller,
      mojo::PendingReceiver<mojom::SpeakerIdEnrollmentController>
          speaker_id_enrollment_controller,
      mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
      mojo::PendingRemote<mojom::MediaDelegate> media_delegate,
      mojo::PendingRemote<mojom::PlatformDelegate> platform_delegate) override;
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<mojom::SpeechRecognitionObserver> observer) override;

 private:
  ServiceController& service_controller() { return *service_controller_; }

  mojo::Receiver<mojom::LibassistantService> receiver_;
  mojo::Remote<mojom::PlatformDelegate> platform_delegate_;

  mojo::RemoteSet<mojom::SpeechRecognitionObserver>
      speech_recognition_observers_;

  // These controllers are part of the platform api which is called from
  // Libassistant, and thus they must outlive |service_controller_|.
  std::unique_ptr<PlatformApi> platform_api_;
  std::unique_ptr<AudioInputController> audio_input_controller_;

  std::unique_ptr<ServiceController> service_controller_;

  // These controllers call Libassistant, and thus they must *not* outlive
  // |service_controller_|.
  std::unique_ptr<ConversationController> conversation_controller_;
  std::unique_ptr<ConversationStateListenerImpl> conversation_state_listener_;
  std::unique_ptr<DisplayController> display_controller_;
  std::unique_ptr<MediaController> media_controller_;
  std::unique_ptr<SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
