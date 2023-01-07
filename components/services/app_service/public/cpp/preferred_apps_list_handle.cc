// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "base/observer_list.h"

namespace apps {

PreferredAppsListHandle::PreferredAppsListHandle() = default;

PreferredAppsListHandle::~PreferredAppsListHandle() {
  for (auto& obs : observers_) {
    obs.OnPreferredAppsListWillBeDestroyed(this);
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
