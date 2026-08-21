// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"
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
  bool IsWebFilteringEnabled() const override;
  bool IsIncognitoModeDisabled() const override;
  bool IsSafeSearchForced() const override;
  bool IsEnabled() const override;
  void RegisterDeviceLevelSyntheticFieldTrials(
      SynteticFieldTrialDelegate& synthetic_field_trial_delegate)
      const override;

  // Low-level interface for state of the underlying settings.
  bool IsBrowserContentFiltersEnabled() const;
  bool IsSearchContentFiltersEnabled() const;

  // Test-only interface to set the state of the content filter observer
  // bridges. Note: this does not alter the actual state of Android Secure
  // Settings, only the JNI bridge's state. Otherwise, tests would leak state to
  // outside environment (eg. Android platform that is hosting the tests).
  void SetBrowserContentFiltersEnabledForTesting(bool enabled);
  void SetSearchContentFiltersEnabledForTesting(bool enabled);

 private:
  // ContentFiltersObserverBridge::Observer:
  void OnContentFiltersObserverChanged() override;

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
