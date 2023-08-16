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
  kGestureFling = 3,
  kGestureScroll = 4,
  kAccel = 5,
  kMaxValue = kAccel,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// MultitaskMenuActionType in /src/tools/metrics/histograms/enums.xml.
enum class MultitaskMenuActionType {
  kHalfSplitButton = 0,
  kFullscreenButton = 1,
  kPartialSplitButton = 2,
  kFloatButton = 3,
  kMaxValue = kFloatButton,
};

constexpr char kPartialSplitDurationHistogramName[] =
    "Ash.Window.PartialSplitDuration";

// Used to record when the user pressed the fullscreen option in the multitask
// menu.
constexpr char kFullscreenUserAction[] = "MultitaskMenu_Fullscreen";

// Used to record when the user pressed the exit fullscreen option in the
// multitask menu.
constexpr char kExitFullscreenUserAction[] = "MultitaskMenu_Exit_Fullscreen";

// Used to record when the user pressed the float option in the multitask menu.
constexpr char kFloatUserAction[] = "MultitaskMenu_Float";

// Used to record when the user pressed the unfloat option in the multitask
// menu.
constexpr char kUnFloatUserAction[] = "MultitaskMenu_UnFloat";

// Used to record when the user half splits to primary direction.
constexpr char kHalfSplitPrimaryUserAction[] =
    "MultitaskMenu_HalfSplit_Primary";

// Used to record when the user half splits to secondary direction.
constexpr char kHalfSplitSecondaryUserAction[] =
    "MultitaskMenu_HalfSplit_Secondary";

// Used to record when the user partial splits to one third.
constexpr char kPartialSplitOneThirdUserAction[] =
    "MultitaskMenu_PartialSplit_OneThird";

// Used to record when the user partial splits to two thirds.
constexpr char kPartialSplitTwoThirdsUserAction[] =
    "MultitaskMenu_PartialSplit_TwoThirds";

// Gets the proper histogram name based on whether the user is in tablet mode or
// not.
std::string GetEntryTypeHistogramName();
std::string GetActionTypeHistogramName();

// Records the method the user used to show the multitask menu.
void RecordMultitaskMenuEntryType(MultitaskMenuEntryType entry_type);

// Records the action the user took within the multitask menu.
void RecordMultitaskMenuActionType(MultitaskMenuActionType action_type);

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_METRICS_H_
