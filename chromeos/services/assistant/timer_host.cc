// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/timer_host.h"

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/check.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/libassistant/public/cpp/assistant_timer.h"
#include "libassistant/shared/internal_api/alarm_timer_manager.h"
#include "libassistant/shared/internal_api/alarm_timer_types.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace assistant {

namespace {

AssistantTimerState GetTimerState(assistant_client::Timer::State state) {
  switch (state) {
    case assistant_client::Timer::State::UNKNOWN:
      return AssistantTimerState::kUnknown;
    case assistant_client::Timer::State::SCHEDULED:
      return AssistantTimerState::kScheduled;
    case assistant_client::Timer::State::PAUSED:
      return AssistantTimerState::kPaused;
    case assistant_client::Timer::State::FIRED:
      return AssistantTimerState::kFired;
  }
}

std::vector<AssistantTimer> GetTimers(
    assistant_client::AlarmTimerManager& timer_manager) {
  std::vector<AssistantTimer> result;
  for (const auto& event : timer_manager.GetAllEvents()) {
    // Note that we currently only handle timers, alarms are unsupported.
    if (event.type != assistant_client::AlarmTimerEvent::TIMER)
      continue;

    // We always handle timers that have fired. Only for timers v2, however, do
    // we handle scheduled/paused timers so we can represent those states in UI.
    if (event.timer_data.state != assistant_client::Timer::State::FIRED &&
        !features::IsTimersV2Enabled()) {
      continue;
    }

    AssistantTimer timer;
    timer.id = event.timer_data.timer_id;
    timer.label = event.timer_data.label;
    timer.state = GetTimerState(event.timer_data.state);
    timer.original_duration = base::TimeDelta::FromMilliseconds(
        event.timer_data.original_duration_ms);

    // LibAssistant provides |fire_time_ms| as an offset from unix epoch.
    timer.fire_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(event.timer_data.fire_time_ms);

    // If the |timer| is paused, LibAssistant will specify the amount of time
    // remaining. Otherwise we calculate it based on |fire_time|.
    timer.remaining_time = timer.state == AssistantTimerState::kPaused
                               ? base::TimeDelta::FromMilliseconds(
                                     event.timer_data.remaining_duration_ms)
                               : timer.fire_time - base::Time::Now();

    result.push_back(timer);
  }

  return result;
}

assistant_client::AlarmTimerManager& GetAlarmTimerManager() {
  auto* api = LibassistantV1Api::Get();
  DCHECK(api);
  auto* result = api->assistant_manager_internal()->GetAlarmTimerManager();
  DCHECK(result);
  return *result;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TimerListener
////////////////////////////////////////////////////////////////////////////////

// Helper that listens to Libassistant timer events, and forwards this
// information to controller::OnTimerStateChanged().
class TimerHost::TimerListener {
 public:
  explicit TimerListener(ash::AssistantAlarmTimerController* controller)
      : controller_(*controller),
        main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  TimerListener(const TimerListener&) = delete;
  TimerListener& operator=(const TimerListener&) = delete;
  ~TimerListener() = default;

  void Start() {
    // We always want to know when a timer has started ringing.
    GetAlarmTimerManager().RegisterRingingStateListener(
        [task_runner = main_task_runner_,
         weak_ptr = weak_factory_.GetWeakPtr()]() {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(&TimerListener::OnAlarmTimerStateChanged,
                             weak_ptr));
        });

    if (features::IsTimersV2Enabled()) {
      // In timers v2, we also want to know when timers are scheduled,
      // updated, and/or removed so that we can represent those states
      // in UI.
      GetAlarmTimerManager().RegisterTimerActionListener(
          [task_runner = main_task_runner_,
           weak_ptr = weak_factory_.GetWeakPtr()](
              assistant_client::AlarmTimerManager::EventActionType ignore) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&TimerListener::OnAlarmTimerStateChanged,
                               weak_ptr));
          });

      // Force sync initial alarm/timer state.
      OnAlarmTimerStateChanged();
    }
  }

  void Stop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Notify our alarm/timer controller to clear its cache to remain in sync
    // with LibAssistant.
    controller_.OnTimerStateChanged({});
  }

 private:
  void OnAlarmTimerStateChanged() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    controller_.OnTimerStateChanged(GetTimers(GetAlarmTimerManager()));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  ash::AssistantAlarmTimerController& controller_
      GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<TimerListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// TimerHost
////////////////////////////////////////////////////////////////////////////////

TimerHost::TimerHost(ServiceContext* context) : context_(*context) {
  DCHECK(context);
}

TimerHost::~TimerHost() = default;

void TimerHost::Start() {
  timer_listener_ =
      std::make_unique<TimerListener>(assistant_alarm_timer_controller());

  timer_listener_->Start();
}

void TimerHost::Stop() {
  if (timer_listener_) {
    timer_listener_->Stop();
    timer_listener_.reset();
  }
}

bool TimerHost::IsStopped() const {
  return (timer_listener_ == nullptr);
}

void TimerHost::AddTimeToTimer(const std::string& id,
                               base::TimeDelta duration) {
  if (IsStopped())
    return;

  GetAlarmTimerManager().AddTimeToTimer(id, duration.InSeconds());
}

void TimerHost::PauseTimer(const std::string& id) {
  if (IsStopped())
    return;

  GetAlarmTimerManager().PauseTimer(id);
}

void TimerHost::RemoveAlarmOrTimer(const std::string& id) {
  if (IsStopped())
    return;

  GetAlarmTimerManager().RemoveEvent(id);
}

void TimerHost::ResumeTimer(const std::string& id) {
  if (IsStopped())
    return;

  GetAlarmTimerManager().ResumeTimer(id);
}

ash::AssistantAlarmTimerController*
TimerHost::assistant_alarm_timer_controller() {
  auto* result = context_.assistant_alarm_timer_controller();
  DCHECK(result);
  return result;
}

}  // namespace assistant
}  // namespace chromeos
