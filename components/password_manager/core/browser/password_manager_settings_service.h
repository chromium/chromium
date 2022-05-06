// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

// Service used to access the password manager settings.
class PasswordManagerSettingsService : public KeyedService {
 protected:
  ~PasswordManagerSettingsService() override = default;
};
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SETTINGS_SERVICE_H_
