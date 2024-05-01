// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/recent_session_observer.h"

RecentSessionObserver::RecentSessionObserver() = default;
RecentSessionObserver::~RecentSessionObserver() = default;

base::CallbackListSubscription
RecentSessionObserver::AddLowUsageSessionCallback(
    base::RepeatingClosure callback) {
  if (session_since_startup_) {
    callback.Run();
  }
  return callbacks_.Add(std::move(callback));
}

void RecentSessionObserver::NotifyLowUsageSession() {
  session_since_startup_ = true;
  callbacks_.Notify();
}
