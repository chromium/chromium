// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/pref_names.h"

namespace accuracy_tips {
namespace prefs {

// The last time an accuracy tip was shown.
const char kLastAccuracyTipShown[] = "accuracy_tips.last_tip_shown";

// The last time an accuracy tip was shown.
// Alternative pref to simulate behavior for dark launch.
const char kLastAccuracyTipShownDisabledUi[] =
    "accuracy_tips.last_tip_shown_disabled";

// List of |AccuracyTipInteraction| events from previous dialog prompts.
const char kPreviousInteractions[] = "accuracy_tips.previous_interactions";

// List of |AccuracyTipInteraction| events from previous dialog prompts.
// Alternative pref to simulate behavior for dark launch.
const char kPreviousInteractionsDisabledUi[] =
    "accuracy_tips.previous_interactions_disabled";

}  // namespace prefs
}  // namespace accuracy_tips
