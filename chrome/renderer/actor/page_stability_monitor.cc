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
#include "base/types/cxx23_to_underlying.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/paint_stability_monitor.h"
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
  return o << base::to_underlying(state);
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

PageStabilityMonitor::PageStabilityMonitor(RenderFrame& frame,
                                           bool supports_paint_stability)
    : RenderFrameObserver(&frame),
      paint_stability_monitor_(supports_paint_stability
                                   ? PaintStabilityMonitor::MaybeCreate(frame)
                                   : nullptr) {
  CHECK(render_frame());
  CHECK(render_frame()->GetWebFrame());
  starting_request_count_ =
      render_frame()->GetWebFrame()->GetDocument().ActiveResourceRequestCount();
}

PageStabilityMonitor::~PageStabilityMonitor() {
  start_monitoring_delayed_handle_.CancelTask();
}

void PageStabilityMonitor::WaitForStable(const ToolBase& tool,
                                         int32_t task_id,
                                         Journal& journal,
                                         base::OnceClosure callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);
  journal_entry_ = journal.CreatePendingAsyncEntry(
      task_id, "PageStability",
      JournalDetailsBuilder()
          .Add("requests_before", starting_request_count_)
          .Build());

  monitoring_start_delay_ = tool.ExecutionObservationDelay();

  is_stable_callback_ = std::move(callback);

  if (paint_stability_monitor_) {
    paint_stability_monitor_->Start(journal_entry_.get());
  }

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
      JournalDetailsBuilder()
          .Add("transition", PageTransitionGetCoreTransitionString(transition))
          .Build());
  start_monitoring_delayed_handle_.CancelTask();
  paint_stability_monitor_.reset();
  MoveToState(State::kNavigationCommitted);
}

void PageStabilityMonitor::DidFailProvisionalLoad() {
  if (state_ == State::kWaitForNavigation) {
    // TODO(b/436573891): Should this go back to `kStartMonitoring`?
    journal_entry_->EndEntry(
        JournalDetailsBuilder().AddError("DidFailProvisionalLoad").Build());
    paint_stability_monitor_.reset();
    MoveToState(State::kNavigationFailed);
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
          JournalDetailsBuilder()
              .Add("delay", monitoring_start_delay_.InMilliseconds())
              .Build());
      start_monitoring_delayed_handle_ =
          PostCancelableMoveToStateClosure(State::kStartMonitoring,
                                           monitoring_start_delay_)
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
        // If there's a pending navigation, don't monitor paint stability, since
        // commit/fail is the signal we want to use.
        paint_stability_monitor_.reset();
      } else {
        // Race paint stability with network/thread stability, if paint
        // stability is supported.
        if (paint_stability_monitor_) {
          paint_stability_monitor_->WaitForStable(
              base::BindOnce(&PageStabilityMonitor::OnPaintStabilityReached,
                             weak_ptr_factory_.GetWeakPtr()));
        }
        if (after_request_count > starting_request_count_) {
          journal_entry_->Log("WaitForNetworkIdle",
                              JournalDetailsBuilder()
                                  .Add("requests", after_request_count)
                                  .Build());
          next_state = State::kWaitForNetworkIdle;
        } else {
          journal_entry_->Log("WaitForMainThreadIdle");
          next_state = State::kWaitForMainThreadIdle;
        }
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
          MoveToStateClosure(State::kWaitForMainThreadIdle));
      render_frame()->GetWebFrame()->RequestNetworkIdleCallback(
          network_idle_callback_.callback());
      break;
    }
    case State::kWaitForMainThreadIdle: {
      SetTimeout(State::kTimeoutMainThread, GetMainThreadTimeoutDelay());
      render_frame()->GetWebFrame()->PostIdleTask(
          FROM_HERE,
          base::BindOnce(
              [](base::OnceClosure callback, base::TimeTicks unused_deadline) {
                std::move(callback).Run();
              },
              MoveToStateClosure(State::kWaitForVisualStateRequest)));
      break;
    }
    case State::kWaitForVisualStateRequest: {
      WebFrameWidget* widget = render_frame()->GetWebFrame()->FrameWidget();
      if (!widget->InsertVisualStateRequest(
              MoveToStateClosure(State::kMaybeDelayCallback))) {
        journal_entry_->EndEntry(
            JournalDetailsBuilder()
                .AddError("Failed to wait for new frame presentation due to no "
                          "compositor.")
                .Build());
        MoveToState(State::kInvokeCallback);
      }
      break;
    }
    case State::kTimeoutGlobal: {
      journal_entry_->EndEntry(
          JournalDetailsBuilder()
              .AddError("Timed out waiting for page stability.")
              .Build());
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kTimeoutMainThread: {
      journal_entry_->EndEntry(
          JournalDetailsBuilder()
              .AddError("Timed out waiting for page stability - main thread to "
                        "produce a thread.")
              .Build());
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kMaybeDelayCallback: {
      base::TimeDelta callback_invoke_delay =
          features::kGlicActorPageStabilityInvokeCallbackDelay.Get();
      if (callback_invoke_delay.is_zero()) {
        MoveToState(State::kInvokeCallback);
      } else {
        PostMoveToStateClosure(State::kInvokeCallback, callback_invoke_delay)
            .Run();
      }
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
    case State::kNavigationCommitted: {
      MoveToState(State::kMaybeDelayCallback);
      break;
    }
    case State::kNavigationFailed: {
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kPaintStabilityReached:
      MoveToState(State::kInvokeCallback);
      break;
    case State::kDone: {
      CHECK(!is_stable_callback_);
      break;
    }
  }
}

base::OnceClosure PageStabilityMonitor::MoveToStateClosure(State new_state) {
  return base::BindOnce(&PageStabilityMonitor::MoveToState,
                        weak_ptr_factory_.GetWeakPtr(), new_state);
}

base::OnceClosure PageStabilityMonitor::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      MoveToStateClosure(new_state), delay);
}

base::OnceCallback<base::DelayedTaskHandle()>
PageStabilityMonitor::PostCancelableMoveToStateClosure(State new_state,
                                                       base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        return task_runner->PostCancelableDelayedTask(
            base::subtle::PostDelayedTaskPassKey(), FROM_HERE, std::move(task),
            delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      MoveToStateClosure(new_state), delay);
}

void PageStabilityMonitor::SetTimeout(State timeout_type,
                                      base::TimeDelta delay) {
  CHECK(timeout_type == State::kTimeoutGlobal ||
        timeout_type == State::kTimeoutMainThread);
  PostMoveToStateClosure(timeout_type, delay).Run();
}

void PageStabilityMonitor::OnPaintStabilityReached() {
  // Do this in a separate task since paint stability can be reached while
  // transitioning to `State::kStartMonitoring`.
  PostMoveToStateClosure(State::kPaintStabilityReached).Run();
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
              State::kTimeoutGlobal,
              State::kNavigationCommitted}},
          {State::kStartMonitoring, {
              State::kWaitForNavigation,
              State::kPaintStabilityReached,
              State::kWaitForNetworkIdle,
              State::kWaitForMainThreadIdle}},
          {State::kWaitForNavigation, {
              State::kNavigationCommitted,
              State::kNavigationFailed,
              State::kTimeoutGlobal}},
          {State::kWaitForNetworkIdle, {
              State::kWaitForMainThreadIdle,
              State::kPaintStabilityReached,
              State::kTimeoutGlobal,
              State::kNavigationCommitted}},
          {State::kWaitForMainThreadIdle, {
              State::kWaitForVisualStateRequest,
              State::kPaintStabilityReached,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal,
              State::kNavigationCommitted}},
          {State::kWaitForVisualStateRequest, {
              State::kPaintStabilityReached,
              State::kMaybeDelayCallback,
              State::kInvokeCallback,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal}},
          {State::kTimeoutMainThread, {
              State::kInvokeCallback}},
          {State::kTimeoutGlobal, {
              State::kInvokeCallback}},
          {State::kMaybeDelayCallback, {
              State::kPaintStabilityReached,
              State::kInvokeCallback,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal,
              State::kNavigationCommitted}},
          {State::kNavigationCommitted, {
              State::kMaybeDelayCallback}},
          {State::kNavigationFailed, {
              State::kInvokeCallback}},
          {State::kPaintStabilityReached, {
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
