// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/timer_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace ash::assistant {

class AssistantManagerServiceImpl;
class LibassistantServiceHost;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantHost {
 public:
  explicit AssistantHost(AssistantManagerServiceImpl* service);
  AssistantHost(AssistantHost&) = delete;
  AssistantHost& operator=(AssistantHost&) = delete;
  ~AssistantHost();

  void StartLibassistantService(LibassistantServiceHost* host);
  void StopLibassistantService();

  // Returns the controller that manages conversations with Libassistant.
  libassistant::mojom::ConversationController& conversation_controller();

  // Returns the controller that manages display related settings.
  libassistant::mojom::DisplayController& display_controller();

  // Returns the controller that manages media related settings.
  libassistant::mojom::MediaController& media_controller();

  // Returns the controller that manages the lifetime of the service.
  libassistant::mojom::ServiceController& service_controller();

  // Returns the controller that manages Libassistant settings.
  libassistant::mojom::SettingsController& settings_controller();

  // Returns the controller that manages timers.
  libassistant::mojom::TimerController& timer_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

  // Add an observer that will be informed of all speech recognition related
  // updates.
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<libassistant::mojom::SpeechRecognitionObserver>
          observer);

  void AddAuthenticationStateObserver(
      mojo::PendingRemote<libassistant::mojom::AuthenticationStateObserver>
          observer);

  mojo::PendingRemote<libassistant::mojom::AudioInputController>
  ExtractAudioInputController();
  mojo::PendingReceiver<libassistant::mojom::AudioOutputDelegate>
  ExtractAudioOutputDelegate();
  mojo::PendingReceiver<libassistant::mojom::DeviceSettingsDelegate>
  ExtractDeviceSettingsDelegate();
  mojo::PendingReceiver<libassistant::mojom::MediaDelegate>
  ExtractMediaDelegate();
  mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
  ExtractNotificationDelegate();
  mojo::PendingReceiver<libassistant::mojom::PlatformDelegate>
  ExtractPlatformDelegate();
  mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentController>
  ExtractSpeakerIdEnrollmentController();
  mojo::PendingReceiver<libassistant::mojom::TimerDelegate>
  ExtractTimerDelegate();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void LaunchLibassistantService();
  void LaunchLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<libassistant::mojom::LibassistantService>);
  void StopLibassistantServiceOnBackgroundThread();

  void BindControllers();

  // Callback when `LibassistantService` has disconnected, e.g. process crashes.
  void OnRemoteDisconnected();

  // Reset remote controllers etc. for restarts.
  void ResetRemote();

  // Owned by |Service|.
  raw_ptr<AssistantManagerServiceImpl> service_;

  // Owned by |AssistantManagerServiceImpl|.
  raw_ptr<LibassistantServiceHost> libassistant_service_host_;

  mojo::Remote<libassistant::mojom::LibassistantService> libassistant_service_;

  mojo::Remote<libassistant::mojom::ConversationController>
      conversation_controller_;
  mojo::Remote<libassistant::mojom::DisplayController> display_controller_;
  mojo::Remote<libassistant::mojom::MediaController> media_controller_;
  mojo::Remote<libassistant::mojom::ServiceController> service_controller_;
  mojo::Remote<libassistant::mojom::SettingsController> settings_controller_;
  mojo::Remote<libassistant::mojom::TimerController> timer_controller_;

  // Will be unbound after they are extracted.
  mojo::PendingRemote<libassistant::mojom::AudioInputController>
      audio_input_controller_;
  mojo::PendingReceiver<libassistant::mojom::AudioOutputDelegate>
      pending_audio_output_delegate_receiver_;
  mojo::PendingReceiver<libassistant::mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_receiver_;
  mojo::PendingReceiver<libassistant::mojom::MediaDelegate> media_delegate_;
  mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
      notification_delegate_;
  mojo::PendingReceiver<libassistant::mojom::PlatformDelegate>
      platform_delegate_;
  mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
  mojo::PendingReceiver<libassistant::mojom::TimerDelegate> timer_delegate_;

  // The thread on which the Libassistant service runs.
  // Only used to run LibAssistant service without sandbox for development, e.g.
  // with `--no-sandbox`. Background thread is needed because there are blocking
  // calls when start LibAssistant service, e.g. creating directories.
  // Warning: must be the last object, so it is destroyed (and flushed) first.
  // This will prevent use-after-free issues where the background thread would
  // access other member variables after they have been destroyed.
  base::Thread background_thread_{"Assistant background thread"};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_HOST_H_
