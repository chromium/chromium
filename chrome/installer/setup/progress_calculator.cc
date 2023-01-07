// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/progress_calculator.h"

#include <math.h>

#include "base/check_op.h"

// An install operation generally proceeds through the stages in order. A
// progress value is computed assuming all stages take an equal amount of
// time. Diff vs. full installs diverge early on but then rejoin.
int ProgressCalculator::Calculate(installer::InstallerStage stage) const {
  DCHECK_GT(stage, last_stage_);

  // mini_installer.exe has already extracted resources by the time setup.exe
  // does any processing. Figure this takes ~5% of overall time (pure I/O).
  constexpr double kMinProgress = 5.0;
  constexpr double kMaxProgress = 100.0;

  last_stage_ = stage;

  static_assert(installer::NUM_STAGES > 1, "There must be more than one stage");
  double fraction =
      static_cast<double>(stage) / (double{installer::NUM_STAGES} - 1.0);
  return round(((kMaxProgress - kMinProgress) * fraction) + kMinProgress);
}
