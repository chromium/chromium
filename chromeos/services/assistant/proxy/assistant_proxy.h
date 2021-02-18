// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_

#include <memory>

#include "base/threading/thread.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

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
  using MediaController = chromeos::libassistant::mojom::MediaController;

  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  void Initialize(LibassistantServiceHost* host,
                  std::unique_ptr<network::PendingSharedURLLoaderFactory>
                      pending_url_loader_factory);

  // Returns the controller that manages starting and stopping of the Assistant
  // service.
  ServiceControllerProxy& service_controller();

  // Returns the controller that manages conversations with Libassistant.
  ConversationControllerProxy& conversation_controller_proxy();

  // Returns the controller that manages display related settings.
  DisplayController& display_controller();

  // Returns the controller that manages media related settings.
  MediaController& media_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

  // Add an observer that will be informed of all speech recognition related
  // updates.
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::SpeechRecognitionObserver> observer);

  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
  ExtractAudioInputController();
  mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
  ExtractAudioOutputDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
  ExtractMediaDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
  ExtractPlatformDelegate();
  mojo::PendingRemote<
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
  ExtractSpeakerIdEnrollmentController();

 private:
  using AudioInputControllerMojom =
      chromeos::libassistant::mojom::AudioInputController;
  using PlatformDelegateMojom = chromeos::libassistant::mojom::PlatformDelegate;
  using AudioOutputDelegateMojom =
      chromeos::libassistant::mojom::AudioOutputDelegate;
  using ConversationControllerMojom =
      chromeos::libassistant::mojom::ConversationController;
  using DisplayControllerMojom =
      chromeos::libassistant::mojom::DisplayController;
  using MediaControllerMojom = chromeos::libassistant::mojom::MediaController;
  using MediaDelegateMojom = chromeos::libassistant::mojom::MediaDelegate;
  using LibassistantServiceMojom =
      chromeos::libassistant::mojom::LibassistantService;
  using ServiceControllerMojom =
      chromeos::libassistant::mojom::ServiceController;
  using SpeakerIdEnrollmentControllerMojom =
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void LaunchLibassistantService();
  void LaunchLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<LibassistantServiceMojom>);
  void StopLibassistantService();
  void StopLibassistantServiceOnBackgroundThread();

  void BindControllers(LibassistantServiceHost* host,
                       std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           pending_url_loader_factory);

  // Owned by |AssistantManagerServiceImpl|.
  LibassistantServiceHost* libassistant_service_host_ = nullptr;

  mojo::Remote<LibassistantServiceMojom> libassistant_service_remote_;
  mojo::Remote<DisplayControllerMojom> display_controller_remote_;
  mojo::Remote<MediaControllerMojom> media_controller_remote_;

  std::unique_ptr<ConversationControllerProxy> conversation_controller_proxy_;
  std::unique_ptr<ServiceControllerProxy> service_controller_proxy_;

  // Will be unbound after they are extracted.
  mojo::PendingRemote<AudioInputControllerMojom> audio_input_controller_;
  mojo::PendingReceiver<AudioOutputDelegateMojom>
      pending_audio_output_delegate_receiver_;
  mojo::PendingReceiver<MediaDelegateMojom> media_delegate_;
  mojo::PendingReceiver<PlatformDelegateMojom> platform_delegate_;
  mojo::PendingRemote<SpeakerIdEnrollmentControllerMojom>
      speaker_id_enrollment_controller_;

  // The thread on which the Libassistant service runs.
  // Warning: must be the last object, so it is destroyed (and flushed) first.
  // This will prevent use-after-free issues where the background thread would
  // access other member variables after they have been destroyed.
  base::Thread background_thread_{"Assistant background thread"};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
