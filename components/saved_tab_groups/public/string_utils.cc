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

  std::u16string elapsed_time_str = ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
      elapsed_time_since_creation, true);
  return l10n_util::GetStringFUTF16(IDS_SAVED_TAB_GROUPS_CREATION_FORMAT,
                                    elapsed_time_str);
}

}  // namespace tab_groups
