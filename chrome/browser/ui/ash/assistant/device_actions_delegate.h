// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_

#include <string>

#include "chromeos/services/assistant/public/cpp/assistant_service.h"

class DeviceActionsDelegate {
 public:
  virtual ~DeviceActionsDelegate() = default;

  virtual chromeos::assistant::AppStatus GetAndroidAppStatus(
      const std::string& package_name) = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_
