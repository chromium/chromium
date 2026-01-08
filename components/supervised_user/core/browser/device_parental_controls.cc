// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/device_parental_controls.h"

#include <utility>

#include "base/callback_list.h"

namespace supervised_user {

DeviceParentalControls::DeviceParentalControls() = default;
DeviceParentalControls::~DeviceParentalControls() = default;

void DeviceParentalControls::AddObserver(Observer* observer) const {
  observer_list_.AddObserver(observer);
}

void DeviceParentalControls::RemoveObserver(Observer* observer) const {
  observer_list_.RemoveObserver(observer);
}

void DeviceParentalControls::NotifyBrowserContentFiltersChanged() const {
  observer_list_.Notify(
      &Observer::OnAndroidParentalControlsBrowserContentFiltersChanged);
}

void DeviceParentalControls::NotifySearchContentFiltersChanged() const {
  observer_list_.Notify(
      &Observer::OnAndroidParentalControlsSearchContentFiltersChanged);
}

}  // namespace supervised_user
