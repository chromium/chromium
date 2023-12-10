// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "ui/display/screen.h"

namespace chromeos {

constexpr char kEntryTypeHistogramNamePrefix[] =
    "Ash.Float.MultitaskMenuEntryType";

constexpr char kActionTypeHistogramNamePrefix[] =
    "Ash.Float.MultitaskMenuActionType";

std::string GetHistogramNameSuffix() {
  return display::Screen::GetScreen()->InTabletMode() ? ".TabletMode"
                                                      : ".ClamshellMode";
}

std::string GetEntryTypeHistogramName() {
  return std::string(kEntryTypeHistogramNamePrefix)
      .append(GetHistogramNameSuffix());
}

std::string GetActionTypeHistogramName() {
  return std::string(kActionTypeHistogramNamePrefix)
      .append(GetHistogramNameSuffix());
}

void RecordMultitaskMenuEntryType(MultitaskMenuEntryType entry_type) {
  base::UmaHistogramEnumeration(GetEntryTypeHistogramName(), entry_type);
}

void RecordMultitaskMenuActionType(MultitaskMenuActionType action_type) {
  base::UmaHistogramEnumeration(GetActionTypeHistogramName(), action_type);
}

}  // namespace chromeos
