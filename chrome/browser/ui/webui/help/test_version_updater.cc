// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/test_version_updater.h"

TestVersionUpdater::TestVersionUpdater() = default;

TestVersionUpdater::~TestVersionUpdater() = default;

void TestVersionUpdater::CheckForUpdate(StatusCallback callback,
                                        PromoteCallback) {
  callback.Run(status_, progress_, rollback_, powerwash_, version_,
               update_size_, message_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool TestVersionUpdater::IsManagedAutoUpdateEnabled() {
  return true;
}
#endif
