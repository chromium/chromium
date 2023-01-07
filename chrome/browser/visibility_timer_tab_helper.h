// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_
#define CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// At most one of these is attached to each WebContents. It allows posting
// delayed tasks whose timer only counts down whilst the WebContents is visible
// (and whose timer is reset whenever the WebContents stops being visible).
// If multiple tasks are added, they are queued in a dormant state -- their
// timer will not elapse until earlier tasks are completed.
class VisibilityTimerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VisibilityTimerTabHelper> {
 public:
  VisibilityTimerTabHelper(const VisibilityTimerTabHelper&&) = delete;
  VisibilityTimerTabHelper& operator=(const VisibilityTimerTabHelper&&) =
      delete;

  ~VisibilityTimerTabHelper() override;

  // Runs |task| after the WebContents has been visible for a consecutive
  // duration of at least |visible_delay|.
  void PostTaskAfterVisibleDelay(const base::Location& from_here,
                                 base::OnceClosure task,
                                 base::TimeDelta visible_delay);

  // WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  struct Task;
  friend class content::WebContentsUserData<VisibilityTimerTabHelper>;
  explicit VisibilityTimerTabHelper(content::WebContents* contents);

  void RunTask(base::OnceClosure task);
  void StartNextTaskTimer();

  base::OneShotTimer timer_;
  base::circular_deque<Task> task_queue_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_
