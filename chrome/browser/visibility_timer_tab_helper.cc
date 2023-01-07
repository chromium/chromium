// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visibility_timer_tab_helper.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(VisibilityTimerTabHelper);

struct VisibilityTimerTabHelper::Task {
  base::TimeDelta visible_delay;
  base::Location from_here;
  base::OnceClosure task;
};

VisibilityTimerTabHelper::~VisibilityTimerTabHelper() = default;

void VisibilityTimerTabHelper::PostTaskAfterVisibleDelay(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta visible_delay) {
  if (web_contents()->IsBeingDestroyed())
    return;

  task_queue_.push_back({visible_delay, from_here, std::move(task)});

  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE &&
      task_queue_.size() == 1) {
    StartNextTaskTimer();
  }
}

void VisibilityTimerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!task_queue_.empty()) {
    if (visibility == content::Visibility::VISIBLE)
      StartNextTaskTimer();
    else
      timer_.Stop();
  }
}

VisibilityTimerTabHelper::VisibilityTimerTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<VisibilityTimerTabHelper>(*contents) {}

void VisibilityTimerTabHelper::RunTask(base::OnceClosure task) {
  DCHECK_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);

  task_queue_.pop_front();
  if (!task_queue_.empty())
    StartNextTaskTimer();

  std::move(task).Run();
}

void VisibilityTimerTabHelper::StartNextTaskTimer() {
  Task& task = task_queue_.front();
  DCHECK(task.task);

  // Split the callback, as we might need to use it again if the timer is
  // stopped.
  auto callback_pair = base::SplitOnceCallback(std::move(task.task));
  task.task = std::move(callback_pair.first);

  // Safe to use Unretained, as destroying |this| will destroy timer_,
  // hence cancelling the callback.
  timer_.Start(
      task.from_here, task.visible_delay,
      base::BindOnce(&VisibilityTimerTabHelper::RunTask, base::Unretained(this),
                     std::move(callback_pair.second)));
}
