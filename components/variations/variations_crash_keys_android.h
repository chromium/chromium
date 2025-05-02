// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_ANDROID_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_ANDROID_H_

#include "base/component_export.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/variations/variations_crash_keys.h"

namespace variations {

// On a separate background thread, report the provided Finch experiments to
// Android by saving the Finch experiments to a file on disk AND saving the hash
// of the file to the process state summary.
// When an ANR happens, the Android app will be forcibly closed. When the app is
// launched again, it can query the process state summary and get the hash of
// the Finch experiments. It will then read each file on disk, find the file
// that matches the hash, and add the list of experiments to the ANR report.
COMPONENT_EXPORT(VARIATIONS)
void SaveVariationsForAnrReporting(
    base::CancelableTaskTracker* tracker,
    scoped_refptr<base::SequencedTaskRunner> runner,
    ExperimentListInfo info);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_ANDROID_H_
