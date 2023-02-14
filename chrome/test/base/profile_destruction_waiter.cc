// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/profile_destruction_waiter.h"

ProfileDestructionWaiter::ProfileDestructionWaiter(Profile* profile) {
  profile_observer_.Observe(profile);
}

ProfileDestructionWaiter::~ProfileDestructionWaiter() = default;

void ProfileDestructionWaiter::Wait() {
  run_loop_.Run();
}

void ProfileDestructionWaiter::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK(!destroyed_);
  destroyed_ = true;
  run_loop_.Quit();
  profile_observer_.Reset();
}
