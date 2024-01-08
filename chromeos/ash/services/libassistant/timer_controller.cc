// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/timer_controller.h"

#include "base/memory/raw_ref.h"
#include "base/thread_annotations.h"
#include "build/buildflag.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/utils/timer_utils.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"

namespace ash::libassistant {

////////////////////////////////////////////////////////////////////////////////
// TimerListener
////////////////////////////////////////////////////////////////////////////////

// Helper that listens to Libassistant timer events, and forwards this
// information to controller::OnTimerStateChanged().
class TimerController::TimerListener
    : public GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest> {
 public:
  TimerListener(AssistantClient* assistant_client,
                mojom::TimerDelegate* delegate)
      : assistant_client_(*assistant_client), delegate_(*delegate) {}
  TimerListener(const TimerListener&) = delete;
  TimerListener& operator=(const TimerListener&) = delete;
  ~TimerListener() override = default;

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Register as an observer of |AlarmTimerEvent| to get notified on
    // alarm/timer status change, i.e. when timers are scheduled, updated,
    // and/or removed. Status change will be reflected on UI correspondingly.
    assistant_client_->AddAlarmTimerEventObserver(this);

    // Force sync the initial timer state.
    assistant_client_->GetTimers(base::BindOnce(
        &TimerListener::NotifyTimerStatusChanged, weak_factory_.GetWeakPtr()));
  }

  void Stop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Notify our timer delegate to clear its cache to remain in sync with
    // LibAssistant.
    NotifyTimerStatusChanged(/*timers=*/{});
  }

 private:
  // GrpcServicesObserver:
  // Invoked when an alarm/timer event has been received.
  // TODO(meilinw): Besides the list of all current timers, the V2 proto also
  // returns information associated with the timer which the status has changed.
  // Investigate on if we could use that field.
  void OnGrpcMessage(
      const ::assistant::api::OnAlarmTimerEventRequest& request) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Only handle timer event in this timer listener.
    auto& alarm_timer_event = request.event();
    if (alarm_timer_event.has_on_timer_state_changed()) {
      NotifyTimerStatusChanged(ConstructAssistantTimersFromProto(
          alarm_timer_event.on_timer_state_changed().timer_params()));
    }
  }

  // Notify our timer delegate on any timer status change. |timers| contains
  // all the current timers.
  void NotifyTimerStatusChanged(
      const std::vector<assistant::AssistantTimer>& timers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    delegate_->OnTimerStateChanged(timers);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ref<AssistantClient> assistant_client_ GUARDED_BY_CONTEXT(
      sequence_checker_);
  const raw_ref<mojom::TimerDelegate> delegate_ GUARDED_BY_CONTEXT(
      sequence_checker_);

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
  if (assistant_client_)
    assistant_client_->AddTimeToTimer(id, duration);
}

void TimerController::PauseTimer(const std::string& id) {
  if (assistant_client_)
    assistant_client_->PauseTimer(id);
}

void TimerController::RemoveTimer(const std::string& id) {
  if (assistant_client_)
    assistant_client_->RemoveTimer(id);
}

void TimerController::ResumeTimer(const std::string& id) {
  if (assistant_client_)
    assistant_client_->ResumeTimer(id);
}

void TimerController::OnAssistantClientRunning(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
  timer_listener_ =
      std::make_unique<TimerListener>(assistant_client, delegate_.get());
  timer_listener_->Start();
}

void TimerController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  assistant_client_ = nullptr;

  if (timer_listener_) {
    timer_listener_->Stop();
    timer_listener_.reset();
  }
}

}  // namespace ash::libassistant
