// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_

#include <memory>

#include "base/macros.h"

namespace updates {

class UpdateNotificationServiceBridge {
 public:
  // Create the default UpdateNotificationServiceBridge.
  static std::unique_ptr<UpdateNotificationServiceBridge> Create();

  // Launches Chrome activity after user clicked the notification. Launching
  // behavior may be different which depends on |state|.
  virtual void LaunchChromeActivity(int state) = 0;

  virtual ~UpdateNotificationServiceBridge() = default;

  UpdateNotificationServiceBridge(
      const UpdateNotificationServiceBridge& other) = delete;
  UpdateNotificationServiceBridge& operator=(
      const UpdateNotificationServiceBridge& other) = delete;

 protected:
  UpdateNotificationServiceBridge() = default;
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
