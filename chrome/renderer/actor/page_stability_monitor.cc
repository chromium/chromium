// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_monitor.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_base.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace actor {

using ::blink::WebDocument;
using ::blink::WebFrameWidget;
using ::blink::WebLocalFrame;
using ::content::RenderFrame;
using ::content::RenderFrameObserver;

std::ostream& operator<<(std::ostream& o,
                         const PageStabilityMonitor::State& state) {
  return o << static_cast<int>(state);
}

namespace {

// This is a high-level timeout that starts when WaitForStable is called. If it
// isn't completed after this delay it will timeout. This is relatively long
// because it often includes waiting on network.
base::TimeDelta GetGlobalTimeoutDelay() {
  return features::kGlicActorPageStabilityTimeout.Get();
}

// Timeout used when waiting on local work. This can be shorter because it's
// used after network requests are completed.
base::TimeDelta GetMainThreadTimeoutDelay() {
  return features::kGlicActorPageStabilityLocalTimeout.Get();
}

}  // namespace

PageStabilityMonitor::PageStabilityMonitor(RenderFrame& frame)
    : RenderFrameObserver(&frame) {
  CHECK(render_frame());
  CHECK(render_frame()->GetWebFrame());
  starting_request_count_ =
      render_frame()->GetWebFrame()->GetDocument().ActiveResourceRequestCount();
}

PageStabilityMonitor::~PageStabilityMonitor() = default;

void PageStabilityMonitor::WaitForStable(const ToolBase& tool,
                                         int32_t task_id,
                                         Journal& journal,
                                         base::OnceClosure callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);
  journal_entry_ = journal.CreatePendingAsyncEntry(
      task_id, "PageStability",
      absl::StrFormat("RequestsBefore[%d]", starting_request_count_));

  monitoring_start_delay_ = tool.ExecutionObservationDelay();

  is_stable_callback_ = std::move(callback);

  SetTimeout(State::kTimeoutGlobal, GetGlobalTimeoutDelay());
  MoveToState(State::kMonitorStartDelay);
}

void PageStabilityMonitor::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // If a same-RenderFrame navigation was committed a new document will be
  // loaded so finish observing the page. (loading is observed from the browser
  // process).
  journal_entry_->Log(
      "DidCommitProvisionalLoad",
      absl::StrFormat("transition[%s]",
                      PageTransitionGetCoreTransitionString(transition)));
  MoveToState(State::kInvokeCallback);
}

void PageStabilityMonitor::DidFailProvisionalLoad() {
  if (state_ == State::kWaitForNavigation) {
    journal_entry_->Log("DidFailProvisionalLoad"),
        MoveToState(State::kInvokeCallback);
  }
}

void PageStabilityMonitor::OnDestruct() {
  // It's the responsibility of users of this class to ensure it doesn't outlive
  // the RenderFrame. Posted tasks use WeakPtr so render_frame() is guaranteed
  // to be valid.
}

void PageStabilityMonitor::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  DCheckStateTransition(state_, new_state);

  state_ = new_state;
  switch (state_) {
    case State::kInitial: {
      NOTREACHED();
    }
    case State::kMonitorStartDelay: {
      journal_entry_->Log(
          "MonitorStartDelay",
          absl::StrFormat("delay[%dms]",
                          monitoring_start_delay_.InMilliseconds()));
      PostMoveToStateClosure(State::kStartMonitoring, monitoring_start_delay_)
          .Run();
      break;
    }
    case State::kStartMonitoring: {
      WebDocument document = render_frame()->GetWebFrame()->GetDocument();
      int after_request_count = document.ActiveResourceRequestCount();

      State next_state;
      if (render_frame()->IsRequestingNavigation()) {
        journal_entry_->Log("WaitForNavigation");
        next_state = State::kWaitForNavigation;
      } else if (after_request_count > starting_request_count_) {
        journal_entry_->Log(
            "WaitForNetworkIdle",
            absl::StrFormat("Requests[%d]", after_request_count));
        next_state = State::kWaitForNetworkIdle;
      } else {
        journal_entry_->Log("WaitForMainThreadIdle");
        next_state = State::kWaitForMainThreadIdle;
      }

      MoveToState(next_state);
      break;
    }
    case State::kWaitForNavigation: {
      // Do nothing - the state will change from DidCommit|FailProvisionalLoad
      break;
    }
    case State::kWaitForNetworkIdle: {
      network_idle_callback_.Reset(
          PostMoveToStateClosure(State::kWaitForMainThreadIdle));
      render_frame()->GetWebFrame()->RequestNetworkIdleCallback(
          network_idle_callback_.callback());
      break;
    }
    case State::kWaitForMainThreadIdle: {
      SetTimeout(State::kTimeoutMainThread, GetMainThreadTimeoutDelay());
      render_frame()
          ->GetTaskRunner(blink::TaskType::kIdleTask)
          ->PostTask(FROM_HERE,
                     PostMoveToStateClosure(State::kWaitForVisualStateRequest));
      break;
    }
    case State::kWaitForVisualStateRequest: {
      WebFrameWidget* widget = render_frame()->GetWebFrame()->FrameWidget();
      if (!widget->InsertVisualStateRequest(
              PostMoveToStateClosure(State::kInvokeCallback))) {
        journal_entry_->EndEntry(
            "Failed to wait for new frame presentation due to no "
            "compositor.");
        MoveToState(State::kInvokeCallback);
      }
      break;
    }
    case State::kTimeoutGlobal: {
      journal_entry_->EndEntry("Timed out waiting for page stability.");
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kTimeoutMainThread: {
      journal_entry_->EndEntry(
          "Timed out waiting for page stability - main thread to "
          "produce a thread.");
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kInvokeCallback: {
      // Ensure we release the network idle callback slot.
      network_idle_callback_.Cancel();
      // Call the callback on a separate task.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(is_stable_callback_));
      MoveToState(State::kDone);
      break;
    }
    case State::kDone: {
      CHECK(!is_stable_callback_);
      break;
    }
  }
}

base::OnceClosure PageStabilityMonitor::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  base::OnceClosure task =
      base::BindOnce(&PageStabilityMonitor::MoveToState,
                     weak_ptr_factory_.GetWeakPtr(), new_state);
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(task), delay);
}

void PageStabilityMonitor::SetTimeout(State timeout_type,
                                      base::TimeDelta delay) {
  CHECK(timeout_type == State::kTimeoutGlobal ||
        timeout_type == State::kTimeoutMainThread);
  PostMoveToStateClosure(timeout_type, delay).Run();
}

void PageStabilityMonitor::DCheckStateTransition(State old_state,
                                                 State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kMonitorStartDelay}},
          {State::kMonitorStartDelay, {
              State::kStartMonitoring,
              State::kTimeoutGlobal}},
          {State::kStartMonitoring, {
              State::kWaitForNavigation,
              State::kWaitForNetworkIdle,
              State::kWaitForMainThreadIdle}},
          {State::kWaitForNavigation, {
              State::kInvokeCallback,
              State::kTimeoutGlobal}},
          {State::kWaitForNetworkIdle, {
              State::kWaitForMainThreadIdle,
              State::kTimeoutGlobal}},
          {State::kWaitForMainThreadIdle, {
              State::kWaitForVisualStateRequest,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal}},
          {State::kWaitForVisualStateRequest, {
              State::kInvokeCallback,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal}},
          {State::kTimeoutMainThread, {
              State::kInvokeCallback}},
          {State::kTimeoutGlobal, {
              State::kInvokeCallback}},
          {State::kInvokeCallback, {
              State::kDone}}

          // kDone can be entered after various tasks are posted but before
          // they've invoked (e.g. by a timeout). As such we don't restrict what
          // state moves can be attempted from kDone but instead we never
          // transition out of it in the state machine.

          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

}  // namespace actor
