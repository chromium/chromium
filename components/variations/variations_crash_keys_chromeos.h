// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_

#include "base/component_export.h"
#include "base/task/sequenced_task_runner.h"
#include "components/variations/variations_crash_keys.h"

namespace base {
class CancelableTaskTracker;
}

namespace variations {

// On `runner`, reports the provided crash keys to ChromeOS using a
// .variations-list.txt file in the user's home directory, or /home/chronos if
// no user is logged in. Uses `tracker` to cancel any previously posted writes
// that have not started to run.
COMPONENT_EXPORT(VARIATIONS)
void ReportVariationsToChromeOs(base::CancelableTaskTracker* tracker,
                                scoped_refptr<base::SequencedTaskRunner> runner,
                                ExperimentListInfo info);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_
