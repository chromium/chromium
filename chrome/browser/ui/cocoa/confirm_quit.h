// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_H_

#include "base/time/time.h"

class PrefRegistrySimple;

namespace confirm_quit {

// How long the user must hold down Cmd+Q to confirm the quit.
constexpr base::TimeDelta kShowDuration = base::Milliseconds(1500);

// Duration of the window fade out animation.
constexpr base::TimeDelta kWindowFadeOutDuration = base::Milliseconds(200);

// For metrics recording only: How long the user must hold the keys to
// differentitate kDoubleTap from kTapHold.
constexpr base::TimeDelta kDoubleTapTimeDelta = base::Milliseconds(320);

// These numeric values are used in UMA logs; do not change them.  New values
// should be added at the end, above kSampleCount.
enum ConfirmQuitMetric {
  // The user quit without having the feature enabled.
  kNoConfirm = 0,
  // The user held the accelerator for the entire duration.
  kHoldDuration = 1,
  // The user hit the accelerator twice for the accelerated path.
  kDoubleTap = 2,
  // The user tapped the accelerator once and then held it.
  kTapHold = 3,

  kSampleCount
};

// Records the histogram value for the above metric.
void RecordHistogram(ConfirmQuitMetric sample);

// Registers the preference in app-wide local state.
void RegisterLocalState(PrefRegistrySimple* registry);

}  // namespace confirm_quit

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_H_
