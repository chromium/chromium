// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_DRAW_PHASE_H_
#define CHROME_BROWSER_VR_ELEMENTS_DRAW_PHASE_H_

#include <string>

namespace vr {

// Each draw phase is rendered independently in the order specified below.
// TODO(crbug.com/41361860): We don't really need all these draw phases as
// the draw order depends on an element's insert order.
enum DrawPhase : int {
  // kPhaseNone is to be used for elements that do not draw. Eg, layouts.
  kPhaseNone = 0,
  kPhaseBackground,
  kPhaseForeground,
  kPhaseOverlayForeground,
  kNumDrawPhases = kPhaseOverlayForeground
};

std::string DrawPhaseToString(DrawPhase phase);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_DRAW_PHASE_H_
