// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace ash {

ThrottleObserver::ThrottleObserver(const std::string& name) : name_(name) {}

ThrottleObserver::~ThrottleObserver() = default;

void ThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  DCHECK(!callback_);
  // Make sure active is not set first.
  DCHECK(!active_);
  callback_ = callback;
  context_ = context;
}

void ThrottleObserver::StopObserving() {
  callback_.Reset();
  context_ = nullptr;
}

void ThrottleObserver::SetActive(bool active) {
  active_ = active;
  if (callback_)
    callback_.Run(this);
}

void ThrottleObserver::SetEnforced(bool enforced) {
  enforced_ = enforced;
  if (callback_)
    callback_.Run(this);
}

}  // namespace ash
