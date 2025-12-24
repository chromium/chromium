// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_

#include <string_view>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

// Provides access to Android-specific parental controls settings through JNI
// bridges. Translates the operating system's settings to state of the browser
// features they control.
class AndroidParentalControls : public ContentFiltersObserverBridge::Observer {
 public:
  class Observer {
   public:
    // Note: this is intermediate state of migrating away the Supervised User
    // stack from the pref interface for url filtering. In the target layout,
    // AndroidParentalControls will notify about change of browser
    // feature (such as incognito mode, safe search or web filtering) rather
    // than about change of the underlying parental control setting. Currently,
    // the existing interface is just duplicated.
    // TODO(crbug.com/470298260): replace low-level OS signals with high-level
    // browser feature states.
    virtual void OnAndroidParentalControlsSearchContentFiltersChanged() {}
    virtual void OnAndroidParentalControlsBrowserContentFiltersChanged() {}
  };

  AndroidParentalControls();
  ~AndroidParentalControls() override;
  AndroidParentalControls(const AndroidParentalControls&) = delete;
  const AndroidParentalControls& operator=(const AndroidParentalControls&) =
      delete;

  // Delegates the initialization to the bridges.
  void Init();

  // TODO(crbug.com/470298260): replace low-level OS signals with high-level
  // browser feature states (eg. IsWebFilteringEnabled, IsSafeSearchEnabled,
  // IsIncognitoModeAvailable).
  bool IsBrowserContentFiltersEnabled() const;
  bool IsSearchContentFiltersEnabled() const;

  // Add and remove observers.
  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

  void SetBrowserContentFiltersEnabledForTesting(bool enabled);
  void SetSearchContentFiltersEnabledForTesting(bool enabled);

 private:
  // ContentFiltersObserverBridge::Observer:
  void OnContentFiltersObserverEnabled(std::string_view setting_name) override;
  void OnContentFiltersObserverDisabled(std::string_view setting_name) override;
  void OnContentFiltersObserverChanged(std::string_view setting_name);

  ContentFiltersObserverBridge browser_content_filters_observer_{
      kBrowserContentFiltersSettingName};
  ContentFiltersObserverBridge search_content_filters_observer_{
      kSearchContentFiltersSettingName};

  // Observer list.
  mutable base::ObserverList<Observer>::Unchecked observer_list_;

  base::ScopedObservation<ContentFiltersObserverBridge,
                          ContentFiltersObserverBridge::Observer>
      browser_content_filters_observation_{this};
  base::ScopedObservation<ContentFiltersObserverBridge,
                          ContentFiltersObserverBridge::Observer>
      search_content_filters_observation_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_ANDROID_PARENTAL_CONTROLS_H_
