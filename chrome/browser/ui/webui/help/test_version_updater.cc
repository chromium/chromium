// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/test_version_updater.h"

TestVersionUpdater::TestVersionUpdater() = default;

TestVersionUpdater::~TestVersionUpdater() = default;

void TestVersionUpdater::CheckForUpdate(const StatusCallback& callback,
                                        const PromoteCallback&) {
  callback.Run(status_, progress_, rollback_, version_, update_size_, message_);
}
