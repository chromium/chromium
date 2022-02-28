// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "base/observer_list.h"

namespace apps {

PreferredAppsListHandle::PreferredAppsListHandle() = default;
PreferredAppsListHandle::~PreferredAppsListHandle() = default;

PreferredAppsListHandle::Observer::Observer(PreferredAppsListHandle* handle) {
  Observe(handle);
}

PreferredAppsListHandle::Observer::Observer() = default;
PreferredAppsListHandle::Observer::~Observer() {
  if (handle_) {
    handle_->RemoveObserver(this);
  }
}

void PreferredAppsListHandle::Observer::Observe(
    PreferredAppsListHandle* handle) {
  if (handle == handle_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (handle_) {
    handle_->RemoveObserver(this);
  }
  handle_ = handle;
  if (handle_) {
    handle_->AddObserver(this);
  }
}

void PreferredAppsListHandle::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PreferredAppsListHandle::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace apps
