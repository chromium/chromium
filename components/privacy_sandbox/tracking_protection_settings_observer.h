// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_OBSERVER_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_OBSERVER_H_

namespace privacy_sandbox {

// Used by other components to observe `TrackingProtectionSettings`.
class TrackingProtectionSettingsObserver {
 public:
  TrackingProtectionSettingsObserver() = default;

  TrackingProtectionSettingsObserver(
      const TrackingProtectionSettingsObserver&) = delete;
  TrackingProtectionSettingsObserver& operator=(
      const TrackingProtectionSettingsObserver&) = delete;

  virtual ~TrackingProtectionSettingsObserver() = default;

  // For observation of DNT.
  virtual void OnDoNotTrackEnabledChanged() {}

  // For observation of IP protection.
  virtual void OnIpProtectionEnabledChanged() {}

  // For observation of block all 3PC.
  virtual void OnBlockAllThirdPartyCookiesChanged() {}

  // For observation of tracking protection experiment status.
  virtual void OnTrackingProtection3pcdChanged() {}
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_OBSERVER_H_
