// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_

#include <string_view>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

// Provides access to Android-specific parental controls settings through JNI
// bridges. Translates the operating system's settings to state of the browser
// features they control.
class AndroidParentalControls : public DeviceParentalControls,
                                public ContentFiltersObserverBridge::Observer {
 public:
  AndroidParentalControls();
  ~AndroidParentalControls() override;
  AndroidParentalControls(const AndroidParentalControls&) = delete;
  const AndroidParentalControls& operator=(const AndroidParentalControls&) =
      delete;

  // DeviceParentalControls:
  void Init() override;
  bool IsSafeSearchForced() const override;
  bool IsBrowserContentFiltersEnabled() const override;
  bool IsSearchContentFiltersEnabled() const override;
  void SetBrowserContentFiltersEnabledForTesting(bool enabled) override;
  void SetSearchContentFiltersEnabledForTesting(bool enabled) override;

 private:
  // ContentFiltersObserverBridge::Observer:
  void OnContentFiltersObserverEnabled(std::string_view setting_name) override;
  void OnContentFiltersObserverDisabled(std::string_view setting_name) override;
  void OnContentFiltersObserverChanged(std::string_view setting_name);

  ContentFiltersObserverBridge browser_content_filters_observer_{
      kBrowserContentFiltersSettingName};
  ContentFiltersObserverBridge search_content_filters_observer_{
      kSearchContentFiltersSettingName};

  base::ScopedObservation<ContentFiltersObserverBridge,
                          ContentFiltersObserverBridge::Observer>
      browser_content_filters_observation_{this};
  base::ScopedObservation<ContentFiltersObserverBridge,
                          ContentFiltersObserverBridge::Observer>
      search_content_filters_observation_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_
