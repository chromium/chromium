// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/browser/page_settled_monitor.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace page_content_annotations {

std::ostream& operator<<(std::ostream& o,
                         const PageSettledMonitor::State& state) {
  return o << PageSettledMonitor::StateToString(state);
}

PageSettledMonitor::Delegate::Delegate(
    std::optional<PageSettledMonitor::PageStabilityConfig>
        page_stability_config)
    : page_stability_config_(std::move(page_stability_config)) {}

void PageSettledMonitor::Delegate::OnMilestoneReached(
    Milestone milestone,
    base::OnceClosure resume_callback) {
  CHECK(resume_callback);
  std::move(resume_callback).Run();
}

bool PageSettledMonitor::Delegate::ShouldExcludeAdSubframes() const {
  return base::FeatureList::IsEnabled(
      features::kPageSettledMonitorExcludeAdFrameLoading);
}

bool PageSettledMonitor::Delegate::ShouldSkipVisualStateUpdateForHiddenTabs()
    const {
  return base::FeatureList::IsEnabled(
      features::kPageSettledMonitorSkipAwaitVisualStateForHiddenTabs);
}

base::TimeDelta PageSettledMonitor::Delegate::GetCompletionTimeout() const {
  return features::kObservationDelayTimeout.Get();
}

base::TimeDelta PageSettledMonitor::Delegate::GetLcpDelay() const {
  return features::kObservationDelayLcp.Get();
}

base::TimeDelta PageSettledMonitor::Delegate::GetStartDelay() const {
  if (page_stability_config_.has_value()) {
    return page_stability_config_->start_delay;
  }

  return base::TimeDelta();
}

PageSettledMonitor::PageSettledMonitor(content::RenderFrameHost* target_frame,
                                       std::unique_ptr<Delegate> delegate)
    : content::WebContentsObserver(
          target_frame ? content::WebContents::FromRenderFrameHost(target_frame)
                       : nullptr),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);

  mojo::PendingRemote<mojom::PageStabilityMonitor> pending_remote =
      delegate_->CreatePageStabilityMonitor(target_frame);
  if (pending_remote.is_valid()) {
    page_stability_monitor_remote_.Bind(std::move(pending_remote));
    page_stability_monitor_remote_.set_disconnect_handler(base::BindOnce(
        &PageSettledMonitor::OnMojoDisconnected, base::Unretained(this)));
  }
}

PageSettledMonitor::~PageSettledMonitor() = default;

void PageSettledMonitor::Wait(content::WebContents* web_contents,
                              ReadyCallback callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(callback);

  CHECK(!ready_callback_);
  ready_callback_ = std::move(callback);

  if (!web_contents) {
    MoveToState(State::kDone);
    return;
  }

  Observe(web_contents);

  PostMoveToStateClosure(State::kDidTimeout, delegate_->GetCompletionTimeout())
      .Run();

  MoveToState(State::kWaitForPageStability);
}

void PageSettledMonitor::OnPageStable() {
  if (state_ != State::kWaitForPageStability) {
    return;
  }

  page_stability_monitor_remote_.reset();

  delegate_->OnEvent(Event::kPageStabilized);
  NotifyMilestone(Milestone::kPageStability);
}

void PageSettledMonitor::OnMojoDisconnected() {
  page_stability_monitor_remote_.reset();

  delegate_->OnEvent(Event::kMojoDisconnected);

  if (state_ == State::kInitial) {
    // If Wait hasn't been called, don't enter the state machine yet. Resetting
    // the remote will skip the page stability state.
    return;
  }

  MoveToState(State::kPageStabilityMonitorDisconnected);
}

void PageSettledMonitor::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  // If the WebContents is destroyed, we can't settle further.
  if (!web_contents()) {
    new_state = State::kDone;
  }

  DCheckStateTransition(state_, new_state);

  delegate_->WillMoveToState(new_state);

  state_ = new_state;

  switch (state_) {
    case State::kInitial:
      NOTREACHED();
    case State::kWaitForPageStability:
      if (page_stability_monitor_remote_.is_bound()) {
        // Unretained since `this` owns the pipe.
        page_stability_monitor_remote_->NotifyWhenStable(
            delegate_->GetStartDelay(),
            base::BindOnce(&PageSettledMonitor::OnPageStable,
                           base::Unretained(this)));
      } else {
        NotifyMilestone(Milestone::kPageStability);
      }
      break;
    case State::kPageStabilityMonitorDisconnected:
      NotifyMilestone(Milestone::kPageStability);
      break;
    case State::kWaitForLoadCompletion: {
      bool is_web_contents_loading =
          delegate_->ShouldExcludeAdSubframes()
              ? web_contents()->IsLoadingExcludingAdSubframes()
              : web_contents()->IsLoading();
      if (is_web_contents_loading) {
        // Milestone will be notified from DidStopLoading in this case.
        break;
      }

      NotifyMilestone(Milestone::kLoadCompletion);
      break;
    }
    case State::kWaitForVisualStateUpdate:
      if (delegate_->ShouldSkipVisualStateUpdateForHiddenTabs() &&
          web_contents()->GetVisibility() != content::Visibility::VISIBLE &&
          !web_contents()->IsBeingCaptured()) {
        // If the tab is not visible and not being captured, it won't produce
        // visual updates (and thus no visual state callback will be invoked).
        // We skip this step to avoid waiting indefinitely.
        delegate_->OnEvent(Event::kVisualStateUpdateSkipped);
        NotifyMilestone(Milestone::kVisualStateUpdate);
      } else {
        // TODO(crbug.com/414662842): This should probably ensure an update from
        // all/selected OOPIFS?
        web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
            base::BindOnce(&PageSettledMonitor::OnVisualStateUpdated,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case State::kMaybeDelayForLcp: {
      if (delegate_->GetLcpDelay().is_positive()) {
        // Conservatively, only apply delay if we get a clear signal that LCP
        // has not yet occurred on a trackable webpage. This avoids adding
        // unnecessary delays on pages where LCP is not applicable or
        // where page load metrics are not available.
        if (auto* metrics_observer =
                page_load_metrics::MetricsWebContentsObserver::FromWebContents(
                    web_contents())) {
          if (const page_load_metrics::PageLoadMetricsObserverDelegate*
                  delegate =
                      metrics_observer->GetDelegateForCommittedLoadOrNull()) {
            const page_load_metrics::ContentfulPaintTimingInfo& lcp =
                delegate->GetLargestContentfulPaintHandler()
                    .MergeMainFrameAndSubframes();
            if (!lcp.ContainsValidTime()) {
              PostMoveToStateClosure(State::kDelayForLcp).Run();
              break;
            }
          }
        }
      }
      NotifyMilestone(Milestone::kLcpSettled);
      break;
    }
    case State::kDelayForLcp:
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PageSettledMonitor::OnLcpSettled,
                         weak_ptr_factory_.GetWeakPtr()),
          delegate_->GetLcpDelay());
      break;
    case State::kDidTimeout:
      MoveToState(State::kDone);
      break;
    case State::kDone:
      // The state machine is never entered until Wait is called so a callback
      // must be provided.
      CHECK(ready_callback_);
      page_stability_monitor_remote_.reset();
      std::move(ready_callback_).Run();
      break;
  }
}

void PageSettledMonitor::OnVisualStateUpdated(bool) {
  if (state_ != State::kWaitForVisualStateUpdate) {
    return;
  }

  delegate_->OnEvent(Event::kVisualStateUpdated);
  NotifyMilestone(Milestone::kVisualStateUpdate);
}

void PageSettledMonitor::DidStopLoading() {
  if (state_ != State::kWaitForLoadCompletion ||
      last_notified_milestone_ == Milestone::kLoadCompletion) {
    return;
  }

  delegate_->OnEvent(Event::kLoadCompleted);
  NotifyMilestone(Milestone::kLoadCompletion);
}

void PageSettledMonitor::OnLcpSettled() {
  NotifyMilestone(Milestone::kLcpSettled);
}

void PageSettledMonitor::NotifyMilestone(Milestone milestone) {
  if (state_ == State::kDone || last_notified_milestone_ == milestone) {
    return;
  }

  last_notified_milestone_ = milestone;

  delegate_->OnMilestoneReached(
      milestone, base::BindOnce(&PageSettledMonitor::Resume,
                                weak_ptr_factory_.GetWeakPtr(), milestone));
}

void PageSettledMonitor::Resume(Milestone milestone) {
  if (state_ == State::kDone) {
    return;
  }

  switch (milestone) {
    case Milestone::kPageStability:
      PostMoveToStateClosure(State::kWaitForLoadCompletion).Run();
      break;
    case Milestone::kLoadCompletion:
      PostMoveToStateClosure(State::kWaitForVisualStateUpdate).Run();
      break;
    case Milestone::kVisualStateUpdate:
      PostMoveToStateClosure(State::kMaybeDelayForLcp).Run();
      break;
    case Milestone::kLcpSettled:
      PostMoveToStateClosure(State::kDone).Run();
      break;
  }
}

void PageSettledMonitor::DCheckStateTransition(State old_state,
                                               State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kWaitForPageStability,
               State::kDone}},
          {State::kWaitForPageStability,
              {State::kDidTimeout,
               State::kDone,
               State::kPageStabilityMonitorDisconnected,
               State::kWaitForLoadCompletion}},
          {State::kPageStabilityMonitorDisconnected,
              {State::kDidTimeout,
               State::kDone,
               State::kWaitForLoadCompletion}},
          {State::kWaitForLoadCompletion,
              {State::kDidTimeout,
               State::kDone,
               State::kWaitForVisualStateUpdate}},
          {State::kWaitForVisualStateUpdate,
              {State::kDidTimeout,
               State::kDone,
               State::kMaybeDelayForLcp}},
          {State::kMaybeDelayForLcp,
              {State::kDidTimeout,
               State::kDelayForLcp,
               State::kDone}},
          {State::kDelayForLcp,
              {State::kDidTimeout,
               State::kDone}},
          {State::kDidTimeout,
              {State::kDone}}
          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

std::string_view PageSettledMonitor::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kWaitForPageStability:
      return "WaitForPageStability";
    case State::kPageStabilityMonitorDisconnected:
      return "PageStabilityMonitorDisconnected";
    case State::kWaitForLoadCompletion:
      return "WaitForLoadCompletion";
    case State::kWaitForVisualStateUpdate:
      return "WaitForVisualStateUpdate";
    case State::kMaybeDelayForLcp:
      return "MaybeDelayForLcp";
    case State::kDelayForLcp:
      return "DelayForLcp";
    case State::kDidTimeout:
      return "DidTimeout";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

base::OnceClosure PageSettledMonitor::MoveToStateClosure(State new_state) {
  return base::BindOnce(&PageSettledMonitor::MoveToState,
                        weak_ptr_factory_.GetWeakPtr(), new_state);
}

base::OnceClosure PageSettledMonitor::PostMoveToStateClosure(
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

}  // namespace page_content_annotations
