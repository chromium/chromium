// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

// TODO(b/182389513): Handle the proper plural case for the display text.
std::string GetUnitDisplayText(const std::string& name) {
  static constexpr char kPoundName[] = "Pound";
  constexpr auto kUnitDisplayTextMap =
      base::MakeFixedFlatMap<base::StringPiece, int>(
          {{kPoundName, IDS_UNIT_CONVERSION_POUND_DISPLAY_TEXT}});

  const auto* it = kUnitDisplayTextMap.find(name);
  if (it == kUnitDisplayTextMap.end())
    return name;

  return l10n_util::GetStringUTF8(it->second);
}

}  // namespace quick_answers
