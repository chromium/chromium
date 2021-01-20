// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_

#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/display_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace libassistant {
namespace mojom {
class SpeechRecognitionObserver;
}  // namespace mojom
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {
class CrosDisplayConnection;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class DisplayController : public mojom::DisplayController,
                          public AssistantManagerObserver {
 public:
  explicit DisplayController(mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
                                 speech_recognition_observers);
  DisplayController(const DisplayController&) = delete;
  DisplayController& operator=(const DisplayController&) = delete;
  ~DisplayController() override;

  void Bind(mojo::PendingReceiver<mojom::DisplayController> receiver);

  // mojom::DisplayController implementation:
  void SetArcPlayStoreEnabled(bool enabled) override;
  void SetDeviceAppsEnabled(bool enabled) override;
  void SetRelatedInfoEnabled(bool enabled) override;
  void SetAndroidAppList(std::vector<mojom::AndroidAppInfoPtr> apps) override;

  // AssistantManagerObserver implementation:
  void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

 private:
  class EventObserver;

  mojo::Receiver<mojom::DisplayController> receiver_{this};
  std::unique_ptr<EventObserver> event_observer_;
  std::unique_ptr<assistant::CrosDisplayConnection> display_connection_;

  // Owned by |LibassistantService|.
  mojo::RemoteSet<mojom::SpeechRecognitionObserver>&
      speech_recognition_observers_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONTROLLER_H_
