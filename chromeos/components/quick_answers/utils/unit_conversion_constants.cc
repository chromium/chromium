// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

// TODO(b/323408178): Handle the proper plural case for the display text.
// Also update to pluralize dependent on unit amount (i.e. >= 1).
std::string GetUnitDisplayText(const std::string& name) {
  constexpr auto kUnitDisplayTextMap =
      base::MakeFixedFlatMap<std::string_view, int>(
          {{kPoundName, IDS_UNIT_CONVERSION_POUND_DISPLAY_TEXT}});

  const auto it = kUnitDisplayTextMap.find(name);
  if (it == kUnitDisplayTextMap.end()) {
    return name;
  }

  return l10n_util::GetStringUTF8(it->second);
}

}  // namespace quick_answers
