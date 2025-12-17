// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_SCHEDULER_H_
#define CHROME_BROWSER_UPDATER_SCHEDULER_H_

#include "base/functional/callback_forward.h"

namespace updater {

// Do the periodic tasks right away, invoking `callback` when done. If user
// intervention is needed, calls `prompt`.
void DoPeriodicTasks(base::RepeatingClosure prompt, base::OnceClosure callback);

// Wake up all existing updater instances. May block. Invokes `callback` once
// the wake process exits.
void WakeAllUpdaters(base::OnceClosure callback);

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_SCHEDULER_H_
