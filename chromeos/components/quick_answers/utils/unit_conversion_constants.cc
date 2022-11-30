// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

const char kRuleSetPath[] = "unitConversionResult.conversions";
const char kSourceUnitPath[] = "unitConversionResult.sourceUnit";
const char kSourceAmountPath[] = "unitConversionResult.sourceAmount";
const char kDestAmountPath[] = "unitConversionResult.destAmount";
const char kDestTextPath[] =
    "unitConversionResult.destination.valueAndUnit.rawText";

const char kCategoryPath[] = "category";
const char kConversionRateAPath[] = "conversionToSiA";
const char kResultValueTemplate[] = "%.3f";
const char kNamePath[] = "name";
const char kUnitsPath[] = "units";

constexpr char kPoundName[] = "Pound";

// TODO(b/182389513): Handle the proper plural case for the display text.
std::string GetUnitDisplayText(const std::string& name) {
  constexpr auto kUnitDisplayTextMap =
      base::MakeFixedFlatMap<base::StringPiece, int>(
          {{kPoundName, IDS_UNIT_CONVERSION_POUND_DISPLAY_TEXT}});

  const auto* it = kUnitDisplayTextMap.find(name);
  if (it == kUnitDisplayTextMap.end())
    return name;

  return l10n_util::GetStringUTF8(it->second);
}

}  // namespace quick_answers
