// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/tracker.h"

#include <utility>

namespace feature_engagement {

DisplayLockHandle::DisplayLockHandle(ReleaseCallback callback)
    : release_callback_(std::move(callback)) {}

DisplayLockHandle::~DisplayLockHandle() {
  if (release_callback_.is_null())
    return;

  std::move(release_callback_).Run();
}

Tracker::TriggerDetails::TriggerDetails(bool should_trigger_iph,
                                        bool should_show_snooze)
    : should_trigger_iph_(should_trigger_iph),
      should_show_snooze_(should_show_snooze) {}

Tracker::TriggerDetails::TriggerDetails(const TriggerDetails& trigger_details) =
    default;

Tracker::TriggerDetails::~TriggerDetails() = default;

bool Tracker::TriggerDetails::ShouldShowIph() const {
  return should_trigger_iph_;
}

bool Tracker::TriggerDetails::ShouldShowSnooze() const {
  return should_show_snooze_;
}

}  // namespace feature_engagement
