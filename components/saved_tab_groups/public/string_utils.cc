// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/string_utils.h"

#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace tab_groups {

std::u16string LocalizedElapsedTimeSinceCreation(
    base::TimeDelta elapsed_time_since_creation) {
  if (elapsed_time_since_creation < base::Minutes(1)) {
    return l10n_util::GetStringUTF16(IDS_SAVED_TAB_GROUPS_CREATION_JUST_NOW);
  }

  if (elapsed_time_since_creation < base::Hours(1)) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_SAVED_TAB_GROUPS_CREATION_MINUTES_AGO_FORMAT,
        elapsed_time_since_creation.InMinutes());
  }

  constexpr base::TimeDelta kDay = base::Days(1);

  if (elapsed_time_since_creation < kDay) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_SAVED_TAB_GROUPS_CREATION_HOURS_AGO_FORMAT,
        elapsed_time_since_creation.InHours());
  }

  // An average month is a twelfth of an average year.
  const float average_month_in_days = 365.25 / 12;

  if (elapsed_time_since_creation < average_month_in_days * kDay) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_SAVED_TAB_GROUPS_CREATION_DAYS_AGO_FORMAT,
        elapsed_time_since_creation.InDays());
  }

  if (elapsed_time_since_creation < 365 * kDay) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_SAVED_TAB_GROUPS_CREATION_MONTHS_AGO_FORMAT,
        elapsed_time_since_creation.InDays() / average_month_in_days);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_SAVED_TAB_GROUPS_CREATION_YEARS_AGO_FORMAT,
      elapsed_time_since_creation.InDays() / 365);
}

}  // namespace tab_groups
