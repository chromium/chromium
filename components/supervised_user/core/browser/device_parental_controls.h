// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_

#include "base/callback_list.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"

namespace supervised_user {

// Interface for device-scope platform-specific parental controls that control
// the behavior of selected browser features for the purpose of user
// supervision. Use accessors or subscriber interface to get notified when
// parental controls state changes. Since these are device-level settings, they
// are global and bound to the browser process. Instance of this class is a
// singleton, and can be accessed via
// g_browser_process->device_parental_controls() in Clank or
// ApplicationContext::GetDeviceParentalControls() in iOS.
class DeviceParentalControls {
 public:
  using Callback = base::RepeatingCallback<void(const DeviceParentalControls&)>;

  DeviceParentalControls();
  virtual ~DeviceParentalControls();
  DeviceParentalControls(const DeviceParentalControls&) = delete;
  const DeviceParentalControls& operator=(const DeviceParentalControls&) =
      delete;

  // Any platform-specific initialization that cannot be done at construction
  // time but is required to make instances of this class functional should be
  // performed in here. Initialized in BrowserProcessImpl.
  virtual void Init() {}

  // Returns true if web filtering (url classification) is enabled for the user.
  virtual bool IsWebFilteringEnabled() const = 0;

  // Returns true if incognito mode is disabled for the user (can be overridden
  // by policies).
  virtual bool IsIncognitoModeDisabled() const = 0;

  // Returns true if safe search is forced for the user client-side (can be
  // overridden by policies).
  virtual bool IsSafeSearchForced() const = 0;

  // Returns true if device-level parental controls are enabled on the device.
  virtual bool IsEnabled() const = 0;

  // Registers synthetic field trials that are used to annotate metrics
  // collection.
  virtual void RegisterDeviceLevelSyntheticFieldTrials(
      SynteticFieldTrialDelegate& synthetic_field_trial_delegate) const = 0;

  // Subscribes to parental controls state changes. Immediately calls the
  // callback with the current state.
  base::CallbackListSubscription Subscribe(Callback callback);

 protected:
  void NotifySubscribers();

 private:
  // Subscribers of this feature. Main consumers should be the pref store (to
  // convert device settings to user-facing browser features where the interface
  // is preference-based), the web filter (to convert device settings to
  // features not driven by prefs) and the metrics service (to emit metrics
  // about device settings).
  base::RepeatingCallbackList<Callback::RunType> subscriber_list_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_H_
