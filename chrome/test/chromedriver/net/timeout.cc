// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/timeout.h"

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"

Timeout::Timeout() : start_(base::TimeTicks::Now()) {
}

Timeout::Timeout(const base::TimeDelta& duration) : Timeout() {
  SetDuration(duration);
}

Timeout::Timeout(const base::TimeDelta& duration, const Timeout* outer)
    : Timeout(duration) {
  if (outer && !outer->deadline_.is_null())
    deadline_ = std::min(outer->deadline_, deadline_);
}

void Timeout::SetDuration(const base::TimeDelta& duration) {
  DCHECK(!start_.is_null());
  if (deadline_.is_null()) {
    deadline_ = start_ + duration;
  } else if (deadline_ - start_ != duration) {
    LOG(ERROR) << "Timeout::SetDuration was called with a duration different"
                  " from what was already set: " << duration << " vs. "
               << deadline_ - start_ << " (original).";
    NOTREACHED_IN_MIGRATION();
  }
}

bool Timeout::IsExpired() const {
  return GetRemainingTime() <= base::TimeDelta();
}

base::TimeDelta Timeout::GetDuration() const {
  return !deadline_.is_null() ? deadline_ - start_
                              : base::TimeDelta::Max();
}

base::TimeDelta Timeout::GetRemainingTime() const {
  return !deadline_.is_null() ? deadline_ - base::TimeTicks::Now()
                              : base::TimeDelta::Max();
}
