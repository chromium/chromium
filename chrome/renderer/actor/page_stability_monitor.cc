// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_monitor.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
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
#include "chrome/renderer/actor/network_and_main_thread_stability_monitor.h"
#include "chrome/renderer/actor/page_stability_metrics.h"
#include "chrome/renderer/actor/paint_stability_monitor.h"
#include "chrome/renderer/actor/tool_base.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

namespace actor {

using ::content::RenderFrame;
using ::content::RenderFrameObserver;

std::ostream& operator<<(std::ostream& o,
                         const PageStabilityMonitor::State& state) {
  return o << PageStabilityMonitor::StateToString(state);
}

namespace {

// This is a high-level timeout that starts when NotifyWhenStable is called. If
// it isn't completed after this delay it will timeout. This is relatively long
// because it often includes waiting on network.
base::TimeDelta GetTimeoutDelay() {
  return features::kGlicActorPageStabilityTimeout.Get();
}

// Minimum amount of time to wait for network/main thread work, and paint
// stability.
base::TimeDelta GetMinWait() {
  return features::kGlicActorPageStabilityMinWait.Get();
}

}  // namespace

PageStabilityMonitor::PageStabilityMonitor(content::RenderFrame& frame,
                                           bool supports_paint_stability,
                                           TaskId task_id,
                                           Journal& journal)
    : RenderFrameObserver(&frame),
      task_id_(task_id),
      journal_(journal),
      paint_stability_monitor_(
          supports_paint_stability
              ? PaintStabilityMonitor::MaybeCreate(frame, task_id, journal)
              : nullptr),
      network_and_main_thread_stability_monitor_(
          std::make_unique<NetworkAndMainThreadStabilityMonitor>(frame,
                                                                 task_id,
                                                                 journal)) {}

PageStabilityMonitor::~PageStabilityMonitor() {
  if (state_ == State::kDone) {
    return;
  }

  // If we have a callback, ensure it replies now.
  OnRenderFrameGoingAway();
  Teardown();
}

void PageStabilityMonitor::NotifyWhenStable(base::TimeDelta observation_delay,
                                            NotifyWhenStableCallback callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);

  metrics_ = std::make_unique<PageStabilityMetrics>();
  metrics_->Start();

  if (render_frame_did_go_away_) {
    MoveToState(State::kRenderFrameGoingAway);
    return;
  }

  monitoring_start_delay_ = observation_delay;

  if (paint_stability_monitor_) {
    paint_stability_monitor_->Start(metrics_.get());
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageStabilityMonitor::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      GetTimeoutDelay());

  MoveToState(State::kMonitorStartDelay);
}

void PageStabilityMonitor::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // If a same-RenderFrame navigation was committed a new document will be
  // loaded so finish observing the page. (loading is observed from the browser
  // process). Also we intentionally don't do this for
  // `DidFinishSameDocumentNavigation()` since that appears instant to
  // browser-side load observation and we do want to wait for page stability in
  // same-document navigations. Note: This can probably be removed once
  // RenderDocument ships everywhere.

  // As we may not destroy PageStabilityMonitor, this may happen after `kDone`.
  if (state_ == State::kDone) {
    return;
  }

  journal_->Log(
      task_id_, "PageStability: DidCommitProvisionalLoad",
      JournalDetailsBuilder()
          .Add("transition", PageTransitionGetCoreTransitionString(transition))
          .Build());
  OnRenderFrameGoingAway();
}

void PageStabilityMonitor::DidFailProvisionalLoad() {
  if (state_ == State::kWaitForNavigation) {
    journal_->Log(task_id_, "DidFailProvisionalLoad", {});
    MoveToState(State::kStartMonitoring);
  }
}

void PageStabilityMonitor::DidSetPageLifecycleState(
    blink::BFCacheStateChange bfcache_change) {
  if (bfcache_change != blink::BFCacheStateChange::kStoredToBFCache) {
    return;
  }

  // As we may not clean up PageStabilityMonitor, this may happen after `kDone`.
  if (state_ == State::kDone) {
    return;
  }

  journal_->Log(task_id_, "PageStabilityMonitor Page Frozen", {});
  OnRenderFrameGoingAway();
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

  CHECK(metrics_);
  metrics_->WillMoveToState(new_state);

  journal_entry_.reset();
  journal_entry_ = journal_->CreatePendingAsyncEntry(
      task_id_,
      absl::StrFormat("PageStabilityState: %s", StateToString(new_state)), {});

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
          PostCancelableMoveToStateClosure(State::kWaitForNavigation,
                                           monitoring_start_delay_)
              .Run();
      break;
    }
    case State::kWaitForNavigation: {
      if (!render_frame()->IsRequestingNavigation()) {
        MoveToState(State::kStartMonitoring);
        break;
      }
      // Do nothing - will advance to the next state from
      // DidCommit|FailProvisionalLoad.
      break;
    }
    case State::kStartMonitoring: {
      start_monitoring_time_ = base::TimeTicks::Now();

      // Race paint stability with network/thread stability, if paint
      // stability is supported.
      if (paint_stability_monitor_) {
        paint_stability_monitor_->WaitForStable(
            base::BindOnce(&PageStabilityMonitor::OnPaintStabilityReached,
                           weak_ptr_factory_.GetWeakPtr()));
      }

      CHECK(network_and_main_thread_stability_monitor_);
      network_and_main_thread_stability_monitor_->WaitForStable(
          base::BindOnce(&PageStabilityMonitor::OnNetworkAndMainThreadIdle,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case State::kTimeout: {
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kMonitorCompleted: {
      base::TimeDelta min_wait_time = GetMinWait();

      base::TimeDelta callback_invoke_delay;
      if (min_wait_time.is_positive()) {
        base::TimeDelta elapsed_time =
            base::TimeTicks::Now() - start_monitoring_time_;
        callback_invoke_delay = min_wait_time - elapsed_time;
      }

      if (callback_invoke_delay.is_positive()) {
        callback_invoke_delay_ = callback_invoke_delay;
        MoveToState(State::kDelayCallback);
      } else {
        MoveToState(State::kInvokeCallback);
      }
      break;
    }
    case State::kDelayCallback: {
      CHECK(callback_invoke_delay_.is_positive());
      PostMoveToStateClosure(State::kInvokeCallback, callback_invoke_delay_)
          .Run();
      break;
    }
    case State::kInvokeCallback: {
      CHECK(is_stable_callback_);

      // It's important to run the callback synchronously so a mojo reply is
      // sent before disconnect.
      std::move(is_stable_callback_).Run();

      MoveToState(State::kDone);
      break;
    }
    case State::kRenderFrameGoingAway: {
      CHECK(render_frame_did_go_away_);
      StopMonitoring();

      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kMojoDisconnected:
      // There's no need to invoke the callback as the mojo pipeline has
      // disconnected.
      MoveToState(State::kDone);
      break;
    case State::kDone: {
      // As we may not destroy PageStabilityMonitor, clean up here.
      Teardown();
      break;
    }
  }
}

void PageStabilityMonitor::StopMonitoring() {
  network_and_main_thread_stability_monitor_.reset();
  paint_stability_monitor_.reset();

  CHECK(metrics_);
  metrics_->Flush();
}

void PageStabilityMonitor::Teardown() {
  start_monitoring_delayed_handle_.CancelTask();
  receiver_.reset();
  journal_entry_.reset();
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

void PageStabilityMonitor::OnPaintStabilityReached() {
  CHECK(metrics_);
  metrics_->OnPaintStabilityReached();

  if (!monitoring_complete_) {
    monitoring_complete_ = true;
    MoveToState(State::kMonitorCompleted);
  }
}

void PageStabilityMonitor::OnRenderFrameGoingAway() {
  render_frame_did_go_away_ = true;

  // Don't enter the state machine until NotifyWhenStable is called.
  if (state_ == State::kInitial) {
    return;
  }

  MoveToState(State::kRenderFrameGoingAway);
}

void PageStabilityMonitor::OnTimeout() {
  StopMonitoring();
  MoveToState(State::kTimeout);
}

void PageStabilityMonitor::OnNetworkAndMainThreadIdle() {
  CHECK(metrics_);
  metrics_->OnNetworkAndMainThreadIdle();

  if (!monitoring_complete_) {
    monitoring_complete_ = true;
    MoveToState(State::kMonitorCompleted);
  }
}

void PageStabilityMonitor::DCheckStateTransition(State old_state,
                                                 State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial, {
              State::kMonitorStartDelay,
              State::kRenderFrameGoingAway,
              State::kMojoDisconnected}},
          {State::kMonitorStartDelay, {
              State::kWaitForNavigation,
              State::kTimeout,
              State::kRenderFrameGoingAway,
              State::kMojoDisconnected}},
          {State::kWaitForNavigation, {
              State::kStartMonitoring,
              State::kTimeout,
              State::kRenderFrameGoingAway,
              State::kMojoDisconnected}},
          {State::kStartMonitoring, {
              State::kMonitorCompleted,
              State::kTimeout,
              State::kRenderFrameGoingAway,
              State::kMojoDisconnected}},
          {State::kTimeout, {
              State::kInvokeCallback}},
          {State::kMonitorCompleted, {
              State::kDelayCallback,
              State::kInvokeCallback}},
          {State::kDelayCallback, {
              State::kInvokeCallback,
              State::kTimeout,
              State::kRenderFrameGoingAway,
              State::kMojoDisconnected}},
          {State::kRenderFrameGoingAway, {
              State::kInvokeCallback}},
          {State::kMojoDisconnected, {
              State::kDone}},
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

void PageStabilityMonitor::Bind(
    mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver) {
  CHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));

  // This interface may be disconnected when the browser-side
  // `ObservationDelayController` that owns the remote is destroyed. This could
  // happen when the tool invocation failed and therefore there's no need to
  // wait.
  receiver_.set_disconnect_handler(base::BindOnce(
      &PageStabilityMonitor::OnMojoDisconnected, base::Unretained(this)));
}

void PageStabilityMonitor::OnMojoDisconnected() {
  // Don't enter the state machine until NotifyWhenStable is called.
  if (state_ == State::kInitial) {
    return;
  }

  MoveToState(State::kMojoDisconnected);
}

// static
std::string_view PageStabilityMonitor::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kMonitorStartDelay:
      return "MonitorStartDelay";
    case State::kWaitForNavigation:
      return "WaitForNavigation";
    case State::kStartMonitoring:
      return "StartMonitoring";
    case State::kMonitorCompleted:
      return "MonitorCompleted";
    case State::kTimeout:
      return "Timeout";
    case State::kDelayCallback:
      return "DelayCallback";
    case State::kInvokeCallback:
      return "InvokeCallback";
    case State::kRenderFrameGoingAway:
      return "RenderFrameGoingAway";
    case State::kMojoDisconnected:
      return "MojoDisconnected";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

}  // namespace actor
