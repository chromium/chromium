// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"

#include <utility>

namespace ash::nearby {
NearbyScheduler::NearbyScheduler(OnRequestCallback callback)
    : callback_(std::move(callback)) {}

NearbyScheduler::~NearbyScheduler() = default;

void NearbyScheduler::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  OnStart();
}

void NearbyScheduler::Stop() {
  DCHECK(is_running_);
  is_running_ = false;
  OnStop();
}

void NearbyScheduler::NotifyOfRequest() {
  callback_.Run();
}

}  // namespace ash::nearby
