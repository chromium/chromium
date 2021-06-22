// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/timer_controller.h"
#include "base/thread_annotations.h"
#include "build/buildflag.h"
#include "chromeos/assistant/internal/buildflags.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "chromeos/services/libassistant/public/cpp/assistant_timer.h"
#include "libassistant/shared/internal_api/alarm_timer_manager.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

#if BUILDFLAG(BUILD_LIBASSISTANT_146S)
#include "libassistant/shared/internal_api/alarm_timer_types.h"
#endif  // BUILD_LIBASSISTANT_146S

#if BUILDFLAG(BUILD_LIBASSISTANT_152S)
#include "libassistant/shared/public/alarm_timer_types.h"
#endif  // BUILD_LIBASSISTANT_152S

namespace chromeos {
namespace libassistant {

namespace {

using ::chromeos::assistant::AssistantTimer;
using ::chromeos::assistant::AssistantTimerState;

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TimerListener
////////////////////////////////////////////////////////////////////////////////

// Helper that listens to Libassistant timer events, and forwards this
// information to controller::OnTimerStateChanged().
class TimerController::TimerListener {
 public:
  explicit TimerListener(
      assistant_client::AlarmTimerManager* alarm_timer_manager,
      mojom::TimerDelegate* delegate)
      : alarm_timer_manager_(*alarm_timer_manager),
        delegate_(*delegate),
        main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  TimerListener(const TimerListener&) = delete;
  TimerListener& operator=(const TimerListener&) = delete;
  ~TimerListener() = default;

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We always want to know when a timer has started ringing.
    alarm_timer_manager_.RegisterRingingStateListener(
        [task_runner = main_task_runner_,
         weak_ptr = weak_factory_.GetWeakPtr()]() {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(&TimerListener::OnAlarmTimerStateChanged,
                             weak_ptr));
        });

      // In timers v2, we also want to know when timers are scheduled,
      // updated, and/or removed so that we can represent those states
      // in UI.
      alarm_timer_manager_.RegisterTimerActionListener(
          [task_runner = main_task_runner_,
           weak_ptr = weak_factory_.GetWeakPtr()](
              assistant_client::AlarmTimerManager::EventActionType ignore) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&TimerListener::OnAlarmTimerStateChanged,
                               weak_ptr));
          });

      // Force sync the initial timer state.
      OnAlarmTimerStateChanged();
  }

  void Stop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Notify our timer delegate to clear its cache to remain in sync with
    // LibAssistant.
    delegate_.OnTimerStateChanged({});
  }

 private:
  void OnAlarmTimerStateChanged() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    delegate_.OnTimerStateChanged(GetTimers(alarm_timer_manager_));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  assistant_client::AlarmTimerManager& alarm_timer_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojom::TimerDelegate& delegate_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<TimerListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// TimerController
////////////////////////////////////////////////////////////////////////////////

TimerController::TimerController() = default;
TimerController::~TimerController() = default;

void TimerController::Bind(
    mojo::PendingReceiver<mojom::TimerController> receiver,
    mojo::PendingRemote<mojom::TimerDelegate> delegate) {
  receiver_.Bind(std::move(receiver));
  delegate_.Bind(std::move(delegate));
}

void TimerController::AddTimeToTimer(const std::string& id,
                                     ::base::TimeDelta duration) {
  if (alarm_timer_manager_)
    alarm_timer_manager_->AddTimeToTimer(id, duration.InSeconds());
}

void TimerController::PauseTimer(const std::string& id) {
  if (alarm_timer_manager_)
    alarm_timer_manager_->PauseTimer(id);
}

void TimerController::RemoveTimer(const std::string& id) {
  if (alarm_timer_manager_)
    alarm_timer_manager_->RemoveEvent(id);
}

void TimerController::ResumeTimer(const std::string& id) {
  if (alarm_timer_manager_)
    alarm_timer_manager_->ResumeTimer(id);
}

void TimerController::OnAssistantClientRunning(
    AssistantClient* assistant_client) {
  alarm_timer_manager_ =
      assistant_client->assistant_manager_internal()->GetAlarmTimerManager();

  timer_listener_ =
      std::make_unique<TimerListener>(alarm_timer_manager_, delegate_.get());
  timer_listener_->Start();
}

void TimerController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  alarm_timer_manager_ = nullptr;

  if (timer_listener_) {
    timer_listener_->Stop();
    timer_listener_.reset();
  }
}

}  // namespace libassistant
}  // namespace chromeos
