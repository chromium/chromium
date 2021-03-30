// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TIMER_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TIMER_CONTROLLER_H_

#include <string>

#include "base/time/time.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/timer_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant_client {
class AlarmTimerManager;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

class TimerController : public mojom::TimerController,
                        public AssistantManagerObserver {
 public:
  TimerController();
  TimerController(const TimerController&) = delete;
  TimerController& operator=(const TimerController&) = delete;
  ~TimerController() override;

  void Bind(mojo::PendingReceiver<mojom::TimerController> receiver,
            mojo::PendingRemote<mojom::TimerDelegate> delegate);

  // mojom::TimerController implementation:
  void AddTimeToTimer(const std::string& id,
                      ::base::TimeDelta duration) override;
  void PauseTimer(const std::string& id) override;
  void RemoveTimer(const std::string& id) override;
  void ResumeTimer(const std::string& id) override;

  // AssistantManagerObserver implementation:
  void OnAssistantManagerRunning(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;
  void OnDestroyingAssistantManager(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

 private:
  class TimerListener;

  // Created when Libassistant is running, and destroyed when it stops.
  std::unique_ptr<TimerListener> timer_listener_;
  // Owned by |ServiceController|, set in OnAssistantManagerRunning() and reset
  // in OnDestroyingAssistantManager().
  assistant_client::AlarmTimerManager* alarm_timer_manager_ = nullptr;

  mojo::Receiver<mojom::TimerController> receiver_{this};
  mojo::Remote<mojom::TimerDelegate> delegate_;
};
}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TIMER_CONTROLLER_H_
