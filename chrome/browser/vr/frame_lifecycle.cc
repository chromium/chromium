// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/frame_lifecycle.h"

namespace vr {

namespace {

static UpdatePhase g_phase = kClean;

}  // namespace

UpdatePhase FrameLifecycle::phase() {
  return g_phase;
}

void FrameLifecycle::set_phase(UpdatePhase phase) {
  g_phase = phase;
}

}  // namespace vr
