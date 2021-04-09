// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/assistant/internal/action/assistant_action_observer.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/display_controller.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace libassistant {
namespace mojom {
class SpeechRecognitionObserver;
}  // namespace mojom
}  // namespace libassistant

namespace assistant {
namespace action {
class CrosActionModule;
}  // namespace action
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class DisplayConnectionImpl;

class DisplayController
    : public mojom::DisplayController,
      public AssistantManagerObserver,
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
      const std::vector<::chromeos::assistant::AndroidAppInfo>& apps) override;

  // AssistantManagerObserver implementation:
  void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;
  void OnDestroyingAssistantManager(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

  // chromeos::assistant::action::AssistantActionObserver:
  void OnVerifyAndroidApp(
      const std::vector<chromeos::assistant::AndroidAppInfo>& apps_info,
      const chromeos::assistant::InteractionInfo& interaction) override;

 private:
  class EventObserver;

  // Checks if the requested Android App with |package_name| is available on the
  // device.
  chromeos::assistant::AppStatus GetAndroidAppStatus(
      const std::string& package_name);

  mojo::Receiver<mojom::DisplayController> receiver_{this};
  std::unique_ptr<EventObserver> event_observer_;
  std::unique_ptr<DisplayConnectionImpl> display_connection_;

  // Owned by |LibassistantService|.
  mojo::RemoteSet<mojom::SpeechRecognitionObserver>&
      speech_recognition_observers_;

  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;

  // Owned by |ConversationController|.
  chromeos::assistant::action::CrosActionModule* action_module_ = nullptr;

  // The callbacks from Libassistant are called on a different sequence,
  // so this sequence checker ensures that no other methods are called on the
  // libassistant sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<DisplayController> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
