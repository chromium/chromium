// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/app_stream_manager.h"

namespace ash {
namespace phonehub {

AppStreamManager::AppStreamManager() = default;

AppStreamManager::~AppStreamManager() = default;

void AppStreamManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppStreamManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppStreamManager::NotifyAppStreamUpdate(
    const proto::AppStreamUpdate app_stream_update) {
  for (auto& observer : observer_list_)
    observer.OnAppStreamUpdate(app_stream_update);
}

}  // namespace phonehub
}  // namespace ash
