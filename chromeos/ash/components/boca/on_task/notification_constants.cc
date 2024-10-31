// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/notification_constants.h"

#include "base/containers/flat_set.h"

namespace ash::boca {

base::flat_set<std::string> GetAllowlistedNotificationIdsForLockedMode() {
  return {kOnTaskEnterLockedModeNotificationId, kOnTaskSessionEndNotificationId,
          kOnTaskBundleContentAddedNotificationId,
          kOnTaskBundleContentRemovedNotificationId};
}

}  // namespace ash::boca
