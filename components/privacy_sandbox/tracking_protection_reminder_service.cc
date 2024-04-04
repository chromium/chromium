// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

#include "components/prefs/pref_service.h"

namespace privacy_sandbox {

TrackingProtectionReminderService::TrackingProtectionReminderService(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

}  // namespace privacy_sandbox
