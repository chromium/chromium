// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/android_parental_controls.h"

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

namespace supervised_user {

AndroidParentalControls::AndroidParentalControls() {
  browser_content_filters_observation_.Observe(
      &browser_content_filters_observer_);
  search_content_filters_observation_.Observe(
      &search_content_filters_observer_);
}

void AndroidParentalControls::Init() {
  // TODO(crbug.com/471178506): initialize the bridges lazily.
  browser_content_filters_observer_.Init();
  search_content_filters_observer_.Init();
}

AndroidParentalControls::~AndroidParentalControls() = default;

void AndroidParentalControls::OnContentFiltersObserverEnabled(
    std::string_view setting_name) {
  OnContentFiltersObserverChanged(setting_name);
}

void AndroidParentalControls::OnContentFiltersObserverDisabled(
    std::string_view setting_name) {
  OnContentFiltersObserverChanged(setting_name);
}

void AndroidParentalControls::OnContentFiltersObserverChanged(
    std::string_view setting_name) {
  if (setting_name == browser_content_filters_observer_.GetSettingName()) {
    observer_list_.Notify(
        &Observer::OnAndroidParentalControlsBrowserContentFiltersChanged);
  } else if (setting_name ==
             search_content_filters_observer_.GetSettingName()) {
    observer_list_.Notify(
        &Observer::OnAndroidParentalControlsSearchContentFiltersChanged);
  } else {
    NOTREACHED() << "Unexpected setting name: " << setting_name;
  }
}

bool AndroidParentalControls::IsBrowserContentFiltersEnabled() const {
  return browser_content_filters_observer_.IsEnabled();
}

bool AndroidParentalControls::IsSearchContentFiltersEnabled() const {
  return search_content_filters_observer_.IsEnabled();
}

void AndroidParentalControls::AddObserver(Observer* observer) const {
  observer_list_.AddObserver(observer);
}

void AndroidParentalControls::RemoveObserver(Observer* observer) const {
  observer_list_.RemoveObserver(observer);
}

void AndroidParentalControls::SetBrowserContentFiltersEnabledForTesting(
    bool enabled) {
  browser_content_filters_observer_.SetEnabledForTesting(enabled);
}

void AndroidParentalControls::SetSearchContentFiltersEnabledForTesting(
    bool enabled) {
  search_content_filters_observer_.SetEnabledForTesting(enabled);
}

}  // namespace supervised_user
