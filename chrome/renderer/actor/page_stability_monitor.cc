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
#include "content/public/renderer/render_frame.h"
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
  return features::kGlicActorActorObservationDelay.Get();
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

void PageStabilityMonitor::WaitForStable(base::OnceClosure callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);

  is_stable_callback_ = std::move(callback);

  SetTimeout(State::kTimeoutGlobal, GetGlobalTimeoutDelay());
  MoveToState(State::kStartMonitoring);
}

void PageStabilityMonitor::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // If a same-RenderFrame navigation was committed a new document will be
  // loaded so finish observing the page. (loading is observed from the browser
  // process).
  MoveToState(State::kInvokeCallback);
}

void PageStabilityMonitor::DidFailProvisionalLoad() {
  if (state_ == State::kWaitForNavigation) {
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
    case State::kStartMonitoring: {
      WebDocument document = render_frame()->GetWebFrame()->GetDocument();
      int after_request_count = document.ActiveResourceRequestCount();

      State next_state;
      if (render_frame()->IsRequestingNavigation()) {
        next_state = State::kWaitForNavigation;
      } else if (after_request_count > starting_request_count_) {
        next_state = State::kWaitForNetworkIdle;
      } else {
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
      render_frame()->GetWebFrame()->RequestNetworkIdleCallback(
          PostMoveToStateClosure(State::kWaitForMainThreadIdle));
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
      WebFrameWidget* widget =
          render_frame()->GetWebFrame()->LocalRoot()->FrameWidget();
      if (!widget->InsertVisualStateRequest(
              PostMoveToStateClosure(State::kInvokeCallback))) {
        ACTOR_LOG() << "Failed to wait for new frame presentation due to no "
                       "compositor.";
        MoveToState(State::kInvokeCallback);
      }
      break;
    }
    case State::kTimeoutGlobal: {
      ACTOR_LOG() << "Timed out waiting for page stability.";
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kTimeoutMainThread: {
      ACTOR_LOG() << "Timed out waiting for page stability - main thread to "
                     "produce a thread.";
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kInvokeCallback: {
      std::move(is_stable_callback_).Run();
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
              {State::kStartMonitoring}},
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
