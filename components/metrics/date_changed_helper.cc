// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/date_changed_helper.h"

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

base::Time GetCurrentDate(base::Time now) {
  // Use local midnight to ensure reporting is for calendar days, not UTC, and
  // avoid storing extra information about when this was called.
  return now.LocalMidnight();
}

void UpdateStoredDate(PrefService* prefs,
                      const char* pref_name,
                      base::Time now) {
  prefs->SetTime(pref_name, GetCurrentDate(now));
}

bool IsStoredDateToday(PrefService* prefs,
                       const char* pref_name,
                       base::Time now) {
  base::Time stored_date = prefs->GetTime(pref_name);
  if (stored_date.is_null()) {
    // Consider date unchanged if pref has never been set.
    UpdateStoredDate(prefs, pref_name, GetCurrentDate(now));
    return true;
  }
  // Ignore small changes in midnight such as time zone changes.
  return std::abs((stored_date - GetCurrentDate(now)).InHours()) < 12;
}

}  // namespace

namespace date_changed_helper {

bool HasDateChangedSinceLastCall(PrefService* prefs, const char* pref_name) {
  DCHECK(prefs);
  DCHECK(pref_name);
  base::Time now = base::Time::Now();
  if (IsStoredDateToday(prefs, pref_name, now))
    return false;
  UpdateStoredDate(prefs, pref_name, now);
  return true;
}

void RegisterPref(PrefRegistrySimple* registry, const char* pref_name) {
  registry->RegisterTimePref(pref_name, base::Time());
}

}  // namespace date_changed_helper

}  // namespace metrics
