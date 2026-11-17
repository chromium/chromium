// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_handler_utils.h"

#include <algorithm>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/time_format.h"

namespace organizer_panel {

std::string GetLastActiveElapsedText(const base::Time& last_active_time) {
  if (last_active_time.is_null()) {
    return std::string();
  }
  const base::TimeDelta elapsed =
      std::max(base::TimeDelta(), base::Time::Now() - last_active_time);
  return base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT, elapsed));
}

}  // namespace organizer_panel
