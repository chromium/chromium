// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ui/base/tablet_state.h"

namespace chromeos {

constexpr char kEntryTypeHistogramNamePrefix[] =
    "Ash.Float.MultitaskMenuEntryType";

std::string GetEntryTypeHistogramName() {
  return std::string(kEntryTypeHistogramNamePrefix)
      .append(TabletState::Get()->InTabletMode() ? ".TabletMode"
                                                 : ".ClamshellMode");
}

void RecordMultitaskMenuEntryType(MultitaskMenuEntryType entry_type) {
  base::UmaHistogramEnumeration(GetEntryTypeHistogramName(), entry_type);
}

}  // namespace chromeos
