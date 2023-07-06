// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_scheduler_base.h"

namespace viz {

DisplaySchedulerBase::DisplaySchedulerBase() = default;

DisplaySchedulerBase::~DisplaySchedulerBase() {
  if (damage_tracker_) {
    damage_tracker_->SetDelegate(nullptr);
  }
}

void DisplaySchedulerBase::SetClient(DisplaySchedulerClient* client) {
  client_ = client;
}

void DisplaySchedulerBase::SetDamageTracker(
    DisplayDamageTracker* damage_tracker) {
  DCHECK(!damage_tracker_);
  DCHECK(damage_tracker);
  damage_tracker_ = damage_tracker;
  damage_tracker_->SetDelegate(this);
}

}  // namespace viz
