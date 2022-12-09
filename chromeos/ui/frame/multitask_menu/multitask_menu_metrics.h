// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_METRICS_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_METRICS_H_

#include <string>

namespace chromeos {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// MultitaskMenuEntryType in /src/tools/metrics/histograms/enums.xml.
enum class MultitaskMenuEntryType {
  kFrameSizeButtonHover = 0,
  kFrameSizeButtonLongPress = 1,
  kFrameSizeButtonLongTouch = 2,
  kMaxValue = kFrameSizeButtonLongTouch,
};

// Used to record when the user partial splits to one third.
constexpr char kPartialSplitOneThirdUserAction[] =
    "MultitaskMenu_PartialSplit_OneThird";

// Used to record when the user partial splits to two thirds.
constexpr char kPartialSplitTwoThirdsUserAction[] =
    "MultitaskMenu_PartialSplit_TwoThirds";

// Gets the proper histogram name based on whether the user is in tablet mode or
// not.
std::string GetEntryTypeHistogramName();

// Records the method the user used to show the multitask menu.
void RecordMultitaskMenuEntryType(MultitaskMenuEntryType entry_type);

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_METRICS_H_
