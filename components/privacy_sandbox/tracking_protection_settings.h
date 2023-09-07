// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

class PrefService;

namespace privacy_sandbox {

// A service which provides an interface for observing and reading tracking
// protection settings.
class TrackingProtectionSettings : public KeyedService {
 public:
  explicit TrackingProtectionSettings(PrefService* pref_service);
  ~TrackingProtectionSettings() override;

  // Wrapper functions for kTrackingProtectionLevel pref.
  tracking_protection::TrackingProtectionLevel GetTrackingProtectionLevel()
      const;
  bool IsCustomTrackingProtectionLevel() const;
  bool IsStandardTrackingProtectionLevel() const;

  // Returns whether "do not track" is enabled.
  bool IsDoNotTrackEnabled() const;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
