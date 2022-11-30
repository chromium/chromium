// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ACTIVITY_IMPL_H_
#define CHROME_UPDATER_ACTIVITY_IMPL_H_

#include <string>

#include "chrome/updater/updater_scope.h"

namespace updater {

bool GetActiveBit(UpdaterScope scope, const std::string& id);

void ClearActiveBit(UpdaterScope scope, const std::string& id);

}  // namespace updater

#endif  // CHROME_UPDATER_ACTIVITY_IMPL_H_
