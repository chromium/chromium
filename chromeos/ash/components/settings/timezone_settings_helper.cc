// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/timezone_settings_helper.h"

#include "base/check.h"

namespace ash::system {

const icu::TimeZone* GetKnownTimezoneOrNull(
    const icu::TimeZone& timezone,
    const std::vector<std::unique_ptr<icu::TimeZone>>& timezone_list) {
  const icu::TimeZone* known_timezone = nullptr;
  icu::UnicodeString id, canonical_id;
  timezone.getID(id);
  UErrorCode status = U_ZERO_ERROR;
  icu::TimeZone::getCanonicalID(id, canonical_id, status);
  DCHECK(U_SUCCESS(status));
  for (const auto& entry : timezone_list) {
    if (*entry.get() == timezone)
      return entry.get();
    // Compare the canonical IDs as well.
    // For instance, Asia/Ulan_Bator -> Asia/Ulaanbaatar or
    // Canada/Pacific -> America/Vancouver
    icu::UnicodeString entry_id, entry_canonical_id;
    entry->getID(entry_id);
    icu::TimeZone::getCanonicalID(entry_id, entry_canonical_id, status);
    DCHECK(U_SUCCESS(status));
    if (entry_canonical_id == canonical_id)
      return entry.get();
    // Last resort: If no match is found, the last timezone in the list
    // with matching rules will be returned.
    if (entry->hasSameRules(timezone))
      known_timezone = entry.get();
  }

  // May return null if we did not find a matching timezone in our list.
  return known_timezone;
}

}  // namespace ash::system
