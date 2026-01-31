// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/callback.h"

namespace updater {

// Chrome Updater is supported on Linux for development purposes only; the
// browser should not attempt to schedule the updater's periodic tasks on the
// platform.
void SchedulePeriodicTasks(base::RepeatingClosure prompt) {}

}  // namespace updater
