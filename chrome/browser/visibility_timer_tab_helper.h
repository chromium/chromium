// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_
#define CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class RetainingOneShotTimer;
}

// At most one of these is attached to each WebContents. It allows posting
// delayed tasks whose timer only counts down whilst the WebContents is visible
// (and whose timer is reset whenever the WebContents stops being visible).
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
  friend class content::WebContentsUserData<VisibilityTimerTabHelper>;
  explicit VisibilityTimerTabHelper(content::WebContents* contents);

  void RunTask(base::OnceClosure task);

  base::circular_deque<std::unique_ptr<base::RetainingOneShotTimer>>
      task_queue_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_VISIBILITY_TIMER_TAB_HELPER_H_
