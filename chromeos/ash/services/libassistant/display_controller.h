// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/assistant/internal/action/assistant_action_observer.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace assistant {
namespace action {
class CrosActionModule;
}  // namespace action
}  // namespace assistant
}  // namespace chromeos

namespace ash::libassistant {

namespace mojom {
class SpeechRecognitionObserver;
}

class DisplayConnection;

class DisplayController
    : public mojom::DisplayController,
      public AssistantClientObserver,
      public chromeos::assistant::action::AssistantActionObserver {
 public:
  explicit DisplayController(mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
                                 speech_recognition_observers);
  DisplayController(const DisplayController&) = delete;
  DisplayController& operator=(const DisplayController&) = delete;
  ~DisplayController() override;

  void Bind(mojo::PendingReceiver<mojom::DisplayController> receiver);

  void SetActionModule(
      chromeos::assistant::action::CrosActionModule* action_module);

  // mojom::DisplayController implementation:
  void SetArcPlayStoreEnabled(bool enabled) override;
  void SetDeviceAppsEnabled(bool enabled) override;
  void SetRelatedInfoEnabled(bool enabled) override;
  void SetAndroidAppList(
      const std::vector<assistant::AndroidAppInfo>& apps) override;

  // AssistantClientObserver implementation:
  void OnAssistantClientCreated(AssistantClient* assistant_client) override;
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;

  // chromeos::assistant::action::AssistantActionObserver:
  void OnVerifyAndroidApp(
      const std::vector<assistant::AndroidAppInfo>& apps_info,
      const chromeos::assistant::InteractionInfo& interaction) override;

  DisplayConnection& GetDisplayConnectionForTesting() {
    return *display_connection_.get();
  }

 private:
  class EventObserver;

  // Checks if the requested Android App with |package_name| is available on the
  // device.
  assistant::AppStatus GetAndroidAppStatus(const std::string& package_name);

  mojo::Receiver<mojom::DisplayController> receiver_{this};
  std::unique_ptr<EventObserver> event_observer_;
  std::unique_ptr<DisplayConnection> display_connection_;

  // Owned by |LibassistantService|.
  const raw_ref<mojo::RemoteSet<mojom::SpeechRecognitionObserver>>
      speech_recognition_observers_;

  raw_ptr<AssistantClient> assistant_client_ = nullptr;

  // Owned by |ConversationController|.
  raw_ptr<chromeos::assistant::action::CrosActionModule> action_module_ =
      nullptr;

  // The callbacks from Libassistant are called on a different sequence,
  // so this sequence checker ensures that no other methods are called on the
  // libassistant sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<DisplayController> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
