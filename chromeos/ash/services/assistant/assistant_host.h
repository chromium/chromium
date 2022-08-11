// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_

#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/timer_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace ash::assistant {

class LibassistantServiceHost;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantHost {
 public:
  AssistantHost();
  AssistantHost(AssistantHost&) = delete;
  AssistantHost& operator=(AssistantHost&) = delete;
  ~AssistantHost();

  void Initialize(LibassistantServiceHost* host);

  // Returns the controller that manages conversations with Libassistant.
  chromeos::libassistant::mojom::ConversationController&
  conversation_controller();

  // Returns the controller that manages display related settings.
  chromeos::libassistant::mojom::DisplayController& display_controller();

  // Returns the controller that manages media related settings.
  chromeos::libassistant::mojom::MediaController& media_controller();

  // Returns the controller that manages the lifetime of the service.
  chromeos::libassistant::mojom::ServiceController& service_controller();

  // Returns the controller that manages Libassistant settings.
  chromeos::libassistant::mojom::SettingsController& settings_controller();

  // Returns the controller that manages timers.
  chromeos::libassistant::mojom::TimerController& timer_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

  // Add an observer that will be informed of all speech recognition related
  // updates.
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::SpeechRecognitionObserver> observer);

  void AddAuthenticationStateObserver(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::AuthenticationStateObserver> observer);

  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
  ExtractAudioInputController();
  mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
  ExtractAudioOutputDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::DeviceSettingsDelegate>
  ExtractDeviceSettingsDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
  ExtractMediaDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
  ExtractNotificationDelegate();
  mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
  ExtractPlatformDelegate();
  mojo::PendingRemote<
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
  ExtractSpeakerIdEnrollmentController();
  mojo::PendingReceiver<chromeos::libassistant::mojom::TimerDelegate>
  ExtractTimerDelegate();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void LaunchLibassistantService();
  void LaunchLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<
          chromeos::libassistant::mojom::LibassistantService>);
  void StopLibassistantService();
  void StopLibassistantServiceOnBackgroundThread();

  void BindControllers();

  // Owned by |AssistantManagerServiceImpl|.
  LibassistantServiceHost* libassistant_service_host_ = nullptr;

  mojo::Remote<chromeos::libassistant::mojom::LibassistantService>
      libassistant_service_;

  mojo::Remote<chromeos::libassistant::mojom::ConversationController>
      conversation_controller_;
  mojo::Remote<chromeos::libassistant::mojom::DisplayController>
      display_controller_;
  mojo::Remote<chromeos::libassistant::mojom::MediaController>
      media_controller_;
  mojo::Remote<chromeos::libassistant::mojom::ServiceController>
      service_controller_;
  mojo::Remote<chromeos::libassistant::mojom::SettingsController>
      settings_controller_;
  mojo::Remote<chromeos::libassistant::mojom::TimerController>
      timer_controller_;

  // Will be unbound after they are extracted.
  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
      audio_input_controller_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
      pending_audio_output_delegate_receiver_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_receiver_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
      media_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
      notification_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
      platform_delegate_;
  mojo::PendingRemote<
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::TimerDelegate>
      timer_delegate_;

  // The thread on which the Libassistant service runs.
  // Warning: must be the last object, so it is destroyed (and flushed) first.
  // This will prevent use-after-free issues where the background thread would
  // access other member variables after they have been destroyed.
  base::Thread background_thread_{"Assistant background thread"};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_
