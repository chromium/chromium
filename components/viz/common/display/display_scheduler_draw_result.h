// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_DISPLAY_SCHEDULER_DRAW_RESULT_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_DISPLAY_SCHEDULER_DRAW_RESULT_H_

namespace viz {

enum class DisplaySchedulerDrawResult {
  // Status is not known or not applicable (e.g. client-side finish frame).
  kUnknown,
  // Frame was drawn normally.
  kDrawn,
  // Frame was skipped at deadline, but might be drawn late.
  kMayDrawLate,
  // Frame was drawn late.
  kDrawnLate,
  // Frame was skipped and will not be drawn late.
  kDidNotDraw,
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_DISPLAY_SCHEDULER_DRAW_RESULT_H_
