// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/profile_waiter.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

ProfileWaiter::ProfileWaiter() {
  profile_manager_observer_.Observe(g_browser_process->profile_manager());
}

ProfileWaiter::~ProfileWaiter() = default;

Profile* ProfileWaiter::WaitForProfileAdded() {
  run_loop_.Run();
  return profile_;
}

void ProfileWaiter::OnProfileAdded(Profile* profile) {
  profile_manager_observer_.Reset();
  profile_ = profile;
  run_loop_.Quit();
}
