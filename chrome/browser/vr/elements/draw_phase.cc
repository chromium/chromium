// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/draw_phase.h"

#include <array>
#include <string>

#include "base/check_op.h"

namespace vr {

namespace {

// LINT.IfChange(DrawPhaseType)
static std::array<const char*, kNumDrawPhases + 1> g_draw_phase_strings = {
    "kPhaseNone",
    "kPhaseBackground",
    "kPhaseForeground",
    "kPhaseOverlayForeground",
};
// LINT.ThenChange(//chrome/browser/vr/elements/draw_phase.h:DrawPhaseType)
}  // namespace

std::string DrawPhaseToString(DrawPhase phase) {
  CHECK_GE(kNumDrawPhases, phase);
  return g_draw_phase_strings[phase];
}

}  // namespace vr
