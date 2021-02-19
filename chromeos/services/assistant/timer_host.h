// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TIMER_HOST_H_
#define CHROMEOS_SERVICES_ASSISTANT_TIMER_HOST_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"

namespace ash {
class AssistantAlarmTimerController;
}  // namespace ash

namespace chromeos {
namespace assistant {

class ServiceContext;

// Handles all timer related interactions with Libassistant, which can broadly
// be separated in 2 responsibilities:
//   1) Let Libassistant know about updates to the timers (pause/add time/...).
//   2) Let |AssistantAlarmTimerController| know when Libassistant adds or
//      removes timers.
class TimerHost {
 public:
  explicit TimerHost(ServiceContext* context);
  TimerHost(const TimerHost&) = delete;
  TimerHost& operator=(const TimerHost&) = delete;
  ~TimerHost();

  void Start();
  void Stop();

  void AddTimeToTimer(const std::string& id, base::TimeDelta duration);
  void PauseTimer(const std::string& id);
  void RemoveAlarmOrTimer(const std::string& id);
  void ResumeTimer(const std::string& id);

 private:
  class TimerListener;

  ash::AssistantAlarmTimerController* assistant_alarm_timer_controller();

  bool IsStopped() const;

  std::unique_ptr<TimerListener> timer_listener_;

  // Owned by the parent |Service| which will destroy |this| before
  // |context_|.
  ServiceContext& context_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TIMER_HOST_H_
