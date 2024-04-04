// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace privacy_sandbox {

class TrackingProtectionReminderService : public KeyedService {
 public:
  explicit TrackingProtectionReminderService(PrefService* pref_service);
  ~TrackingProtectionReminderService() override = default;
  TrackingProtectionReminderService(const TrackingProtectionReminderService&) =
      delete;
  TrackingProtectionReminderService& operator=(
      const TrackingProtectionReminderService&) = delete;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox
#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
