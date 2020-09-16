// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visibility_timer_tab_helper.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(VisibilityTimerTabHelper)

VisibilityTimerTabHelper::~VisibilityTimerTabHelper() = default;

void VisibilityTimerTabHelper::PostTaskAfterVisibleDelay(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta visible_delay) {
  if (web_contents()->IsBeingDestroyed())
    return;

  // Safe to use Unretained, as destroying |this| will destroy task_queue_,
  // hence cancelling all timers.
  // RetainingOneShotTimer is used which needs a RepeatingCallback, but we
  // only have it run this callback a single time, and destroy it after.
  task_queue_.push_back(std::make_unique<base::RetainingOneShotTimer>(
      from_here, visible_delay,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&VisibilityTimerTabHelper::RunTask,
                         base::Unretained(this), std::move(task)))));
  DCHECK(!task_queue_.back()->IsRunning());

  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE &&
      task_queue_.size() == 1) {
    task_queue_.front()->Reset();
  }
}

void VisibilityTimerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!task_queue_.empty()) {
    if (visibility == content::Visibility::VISIBLE)
      task_queue_.front()->Reset();
    else
      task_queue_.front()->Stop();
  }
}

VisibilityTimerTabHelper::VisibilityTimerTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

void VisibilityTimerTabHelper::RunTask(base::OnceClosure task) {
  DCHECK_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);

  task_queue_.pop_front();
  if (!task_queue_.empty())
    task_queue_.front()->Reset();

  std::move(task).Run();
}
