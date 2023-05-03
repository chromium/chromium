// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_
#define CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

absl::optional<base::ScopedCFTypeRef<CFDictionaryRef>> CreateWakeLaunchdPlist(
    UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_
