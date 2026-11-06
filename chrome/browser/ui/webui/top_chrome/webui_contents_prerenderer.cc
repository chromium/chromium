// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory_coordinator/traits.h"
#include "base/process/kill.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr base::MemoryConsumerTraits kPrerenderTraits(
    base::MemoryConsumerTraits::ConsumerType::kPassive,
    base::MemoryConsumerTraits::InProcess::kNo);

}  // namespace

WebUIContentsPrerenderer::WebUIContentsPrerenderer(Delegate& delegate,
                                                   const Params& params)
    : delegate_(delegate), params_(params) {
  timer_.SetTaskRunner(
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT}));
  memory_consumer_registration_.emplace(
      params_.consumer_name, kPrerenderTraits, this,
      base::MemoryConsumerRegistration::CheckUnregister::kDisabled);
}

WebUIContentsPrerenderer::~WebUIContentsPrerenderer() = default;

WebUIContentsPrerenderer::TakeOutcome WebUIContentsPrerenderer::Take() {
  if (!spare_) {
    return {nullptr, TakeResult::kNoSpare};
  }
  TakeResult result = TakeResult::kAdopted;
  // A consumer observes its WebContents' renderer only after taking it, so it
  // would not recover a spare whose renderer exited before, for any reason.
  // This checks the spare's own frame: a shared renderer process may have
  // relaunched for other WebContents while this one stays dead.
  if (spare_->GetCrashedStatus() != base::TERMINATION_STATUS_STILL_RUNNING) {
    result = TakeResult::kDroppedCrashed;
  } else if (spare_->CompletedFirstVisuallyNonEmptyPaint()) {
    // First-paint notifications are one-shot, so a consumer that waits for
    // the spare's first paint would never see it.
    result = TakeResult::kDroppedAlreadyPainted;
  } else if (!delegate_->IsSpareCurrent(*spare_)) {
    result = TakeResult::kDroppedStale;
  }
  if (result == TakeResult::kAdopted) {
    return {std::move(spare_), result};
  }
  spare_.reset();
  return {nullptr, result};
}

void WebUIContentsPrerenderer::ScheduleRefill(
    content::WebContents* taken_contents) {
  CancelRefill();
  Observe(taken_contents);

  if (taken_contents && taken_contents->CompletedFirstVisuallyNonEmptyPaint()) {
    StartDelay();
    return;
  }

  // Unretained is safe: `this` owns `timer_`.
  timer_.Start(FROM_HERE, params_.paint_deadline,
               base::BindOnce(&WebUIContentsPrerenderer::StartDelay,
                              base::Unretained(this)));
}

void WebUIContentsPrerenderer::Reset() {
  CancelRefill();
  spare_.reset();
}

void WebUIContentsPrerenderer::DidFirstVisuallyNonEmptyPaint() {
  StartDelay();
}

void WebUIContentsPrerenderer::StartDelay() {
  Observe(nullptr);
  // Unretained is safe: `this` owns `timer_`.
  timer_.Start(FROM_HERE, params_.delay,
               base::BindOnce(&WebUIContentsPrerenderer::Refill,
                              base::Unretained(this)));
}

void WebUIContentsPrerenderer::Refill() {
  if (memory_limit() <= params_.skip_memory_limit) {
    return;
  }
  if (std::unique_ptr<content::WebContents> spare = delegate_->CreateSpare()) {
    spare_ = std::move(spare);
  }
}

void WebUIContentsPrerenderer::CancelRefill() {
  Observe(nullptr);
  timer_.Stop();
}
