// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"

ExclusiveAccessBubble::ExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params)
    : params_(params),
      hide_timeout_(FROM_HERE,
                    kShowTime,
                    base::BindRepeating(&ExclusiveAccessBubble::Hide,
                                        base::Unretained(this))) {
  DCHECK(params.has_download ||
         params.type != EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
}

ExclusiveAccessBubble::~ExclusiveAccessBubble() = default;

void ExclusiveAccessBubble::OnUserInput() {
  // Re-show the bubble if no user input occurred during the snooze period.
  if (base::TimeTicks::Now() > snooze_until_) {
    ShowAndStartTimers();
    return;
  }

  // Restart the snooze period; to only re-show after a period of inactivity.
  snooze_until_ = base::TimeTicks::Now() + kSnoozeTime;
}

void ExclusiveAccessBubble::ShowAndStartTimers() {
  Show();

  // Restart the timer to hide the bubble after a few seconds.
  hide_timeout_.Reset();

  // Restart the snooze period; to only re-show after a period of inactivity.
  snooze_until_ = base::TimeTicks::Now() + kSnoozeTime;
}
