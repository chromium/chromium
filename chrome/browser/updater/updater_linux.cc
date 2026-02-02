// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/updater.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/mojom/updater_service.mojom.h"

namespace updater {

void CheckForUpdate(base::RepeatingCallback<void(const mojom::UpdateState&)>
                        version_updater_callback) {
  mojom::UpdateState state;
  state.state = mojom::UpdateState::State::kUpdateError;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(version_updater_callback, state));
}

// Chrome Updater is supported on Linux for development purposes only; the
// browser should not attempt to schedule the updater's periodic tasks on the
// platform.
void SchedulePeriodicTasks(base::RepeatingClosure prompt) {}

// Does nothing.
void SetActive() {}

}  // namespace updater
