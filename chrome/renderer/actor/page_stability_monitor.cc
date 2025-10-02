// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_monitor.h"

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
#include "chrome/renderer/actor/paint_stability_monitor.h"
#include "chrome/renderer/actor/tool_base.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

namespace actor {

using ::blink::WebDocument;
using ::blink::WebFrameWidget;
using ::blink::WebLocalFrame;
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
base::TimeDelta GetGlobalTimeoutDelay() {
  return features::kGlicActorPageStabilityTimeout.Get();
}

// Timeout used when waiting on local work. This can be shorter because it's
// used after network requests are completed.
base::TimeDelta GetMainThreadTimeoutDelay() {
  return features::kGlicActorPageStabilityLocalTimeout.Get();
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
              : nullptr) {
  CHECK(render_frame());
  CHECK(render_frame()->GetWebFrame());
  starting_request_count_ =
      render_frame()->GetWebFrame()->GetDocument().ActiveResourceRequestCount();

  journal_->Log(task_id_, "PageStability: Created",
                JournalDetailsBuilder()
                    .Add("requests_before", starting_request_count_)
                    .Build());
}

PageStabilityMonitor::~PageStabilityMonitor() {
  if (state_ == State::kDone) {
    return;
  }

  // If we have a callback, ensure it replies now.
  OnRenderFrameGoingAway();
  Cleanup();
}

void PageStabilityMonitor::NotifyWhenStable(base::TimeDelta observation_delay,
                                            NotifyWhenStableCallback callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);

  if (render_frame_did_go_away_) {
    MoveToState(State::kRenderFrameGoingAway);
    return;
  }

  monitoring_start_delay_ = observation_delay;

  if (paint_stability_monitor_) {
    paint_stability_monitor_->Start();
  }

  SetTimeout(State::kTimeoutGlobal, GetGlobalTimeoutDelay());
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
      WebDocument document = render_frame()->GetWebFrame()->GetDocument();
      int after_request_count = document.ActiveResourceRequestCount();
      journal_entry_->Log(
          "Network Requests",
          JournalDetailsBuilder().Add("count", after_request_count).Build());

      State next_state;

      // Race paint stability with network/thread stability, if paint
      // stability is supported.
      if (paint_stability_monitor_) {
        paint_stability_monitor_->WaitForStable(
            base::BindOnce(&PageStabilityMonitor::OnPaintStabilityReached,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      if (after_request_count > starting_request_count_) {
        next_state = State::kWaitForNetworkIdle;
      } else {
        next_state = State::kWaitForMainThreadIdle;
      }

      MoveToState(next_state);
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
      main_thread_idle_callback_.Reset(base::BindOnce(
          [](base::OnceClosure callback, base::TimeTicks unused_deadline) {
            std::move(callback).Run();
          },
          MoveToStateClosure(State::kMaybeDelayCallback)));
      render_frame()->GetWebFrame()->PostIdleTask(
          FROM_HERE, main_thread_idle_callback_.callback());
      break;
    }
    case State::kTimeoutGlobal: {
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kTimeoutMainThread: {
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kMaybeDelayCallback: {
      // Ensure we release the network and main thread idle callback slots.
      network_idle_callback_.Cancel();
      main_thread_idle_callback_.Cancel();

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
      CHECK(is_stable_callback_);

      // Ensure we release the network and main thread idle callback slots.
      network_idle_callback_.Cancel();
      main_thread_idle_callback_.Cancel();
      if (receiver_.is_bound()) {
        // It's important to run the callback synchronously so a mojo reply is
        // sent before disconnect. If done from the state machine we reset the
        // receiver when moving to kDone. Once GeneralPageStability is enabled
        // the mojo is the only caller so we can always invoke synchronously.
        std::move(is_stable_callback_).Run();
      } else {
        // This path is only used when the monitor is called from the renderer
        // and can be removed when GeneralPageStability is fully enabled.
        CHECK_NE(features::kActorGeneralPageStabilityMode.Get(),
                 features::ActorGeneralPageStabilityMode::kAllEnabled);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(is_stable_callback_));
      }
      MoveToState(State::kDone);
      break;
    }
    case State::kRenderFrameGoingAway: {
      CHECK(render_frame_did_go_away_);
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kPaintStabilityReached:
      MoveToState(State::kInvokeCallback);
      break;
    case State::kDone: {
      CHECK(!is_stable_callback_);
      // As we may not destroy PageStabilityMonitor, clean up here.
      Cleanup();
      break;
    }
  }
}

void PageStabilityMonitor::Cleanup() {
  network_idle_callback_.Cancel();
  main_thread_idle_callback_.Cancel();
  start_monitoring_delayed_handle_.CancelTask();
  receiver_.reset();
  paint_stability_monitor_.reset();
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

void PageStabilityMonitor::SetTimeout(State timeout_type,
                                      base::TimeDelta delay) {
  CHECK(timeout_type == State::kTimeoutGlobal ||
        timeout_type == State::kTimeoutMainThread);
  PostMoveToStateClosure(timeout_type, delay).Run();
}

void PageStabilityMonitor::OnPaintStabilityReached() {
  // Do this in a separate task since this callback can be called synchronously
  // when registered.
  // TODO(bokan): It'd be better for PaintStabilityMonitor to post the reply in
  // this case.
  PostMoveToStateClosure(State::kPaintStabilityReached).Run();
}

void PageStabilityMonitor::OnRenderFrameGoingAway() {
  render_frame_did_go_away_ = true;

  // Don't enter the state machine until NotifyWhenStable is called.
  if (state_ == State::kInitial) {
    return;
  }

  MoveToState(State::kRenderFrameGoingAway);
}

void PageStabilityMonitor::DCheckStateTransition(State old_state,
                                                 State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial, {
              State::kMonitorStartDelay,
              State::kRenderFrameGoingAway}},
          {State::kMonitorStartDelay, {
              State::kWaitForNavigation,
              State::kTimeoutGlobal,
              State::kRenderFrameGoingAway}},
          {State::kWaitForNavigation, {
              State::kStartMonitoring,
              State::kTimeoutGlobal,
              State::kRenderFrameGoingAway}},
          {State::kStartMonitoring, {
              State::kWaitForNetworkIdle,
              State::kWaitForMainThreadIdle}},
          {State::kWaitForNetworkIdle, {
              State::kWaitForMainThreadIdle,
              State::kPaintStabilityReached,
              State::kTimeoutGlobal,
              State::kRenderFrameGoingAway}},
          {State::kWaitForMainThreadIdle, {
              State::kMaybeDelayCallback,
              State::kPaintStabilityReached,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal,
              State::kRenderFrameGoingAway}},
          {State::kTimeoutMainThread, {
              State::kInvokeCallback}},
          {State::kTimeoutGlobal, {
              State::kInvokeCallback}},
          {State::kMaybeDelayCallback, {
              State::kPaintStabilityReached,
              State::kInvokeCallback,
              State::kTimeoutMainThread,
              State::kTimeoutGlobal,
              State::kRenderFrameGoingAway}},
          {State::kRenderFrameGoingAway, {
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

void PageStabilityMonitor::Bind(
    mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver) {
  CHECK_NE(features::kActorGeneralPageStabilityMode.Get(),
           features::ActorGeneralPageStabilityMode::kDisabled);

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
  journal_->Log(task_id_, "OnMojoDisconnected",
                JournalDetailsBuilder().Add("state", state_).Build());
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
    case State::kWaitForNetworkIdle:
      return "WaitForNetworkIdle";
    case State::kWaitForMainThreadIdle:
      return "WaitForMainThreadIdle";
    case State::kTimeoutGlobal:
      return "TimeoutGlobal";
    case State::kTimeoutMainThread:
      return "TimeoutMainThread";
    case State::kMaybeDelayCallback:
      return "MaybeDelayCallback";
    case State::kInvokeCallback:
      return "InvokeCallback";
    case State::kRenderFrameGoingAway:
      return "RenderFrameGoingAway";
    case State::kPaintStabilityReached:
      return "PaintStabilityReached";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

}  // namespace actor
