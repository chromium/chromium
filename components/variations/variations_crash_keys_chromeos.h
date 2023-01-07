// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_

#include "base/component_export.h"
#include "base/task/sequenced_task_runner.h"
#include "components/variations/variations_crash_keys.h"

namespace variations {

// On a separate thread, report the provided crash keys to Chrome OS using a
// .variant-list.txt in the user's home directory, or /home/chronos if no user
// is logged in.
COMPONENT_EXPORT(VARIATIONS)
void ReportVariationsToChromeOs(scoped_refptr<base::SequencedTaskRunner> runner,
                                ExperimentListInfo info);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_CHROMEOS_H_
