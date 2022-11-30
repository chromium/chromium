// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/draw_phase.h"

namespace vr {

namespace {

static const char* g_draw_phase_strings[] = {
    "kPhaseNone",       "kPhaseBackground",        "kPhaseBackplanes",
    "kPhaseForeground", "kPhaseOverlayForeground",
};

static_assert(
    kNumDrawPhases + 1 == std::size(g_draw_phase_strings),
    "Mismatch between the DrawPhase enum and the corresponding strings");

}  // namespace

std::string DrawPhaseToString(DrawPhase phase) {
  return g_draw_phase_strings[phase];
}

}  // namespace vr
