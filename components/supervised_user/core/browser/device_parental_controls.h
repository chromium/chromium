// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_

#include <string_view>

#include "base/observer_list.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"

namespace supervised_user {

// Interface for device-scope platform-specific parental controls that control
// the behavior of selected browser features for the purpose of user
// supervision. Use accessors or observer interface to get notified when
// parental controls state changes. Since these are device-level settings, they
// are global and bound to the browser process. Instance of this class is a
// singleton, and can be accessed via
// g_browser_process->device_parental_controls() in Clank or
// ApplicationContext::GetDeviceParentalControls() in iOS.
class DeviceParentalControls {
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

  // Any initialization that cannot be done at construction time but is required
  // to make instances of this class functional should be performed in here.
  // Initialized with GlobalFeatures.
  virtual void Init() {}

  // Returns true if safe search is forced for the user client-side (can be
  // overridden by policies).
  virtual bool IsSafeSearchForced() const = 0;

  // Temporary migration-time interface to support Android-specific parental
  // controls knobs.
  virtual bool IsBrowserContentFiltersEnabled() const = 0;
  virtual bool IsSearchContentFiltersEnabled() const = 0;

  // Testing interfaces.
  virtual void SetBrowserContentFiltersEnabledForTesting(bool enabled) {}
  virtual void SetSearchContentFiltersEnabledForTesting(bool enabled) {}

  // Add and remove observers.
  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

  DeviceParentalControls();
  virtual ~DeviceParentalControls();
  DeviceParentalControls(const DeviceParentalControls&) = delete;
  const DeviceParentalControls& operator=(const DeviceParentalControls&) =
      delete;

 protected:
  void NotifySearchContentFiltersChanged() const;
  void NotifyBrowserContentFiltersChanged() const;

 private:
  mutable base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_
