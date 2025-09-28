// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// Manages settings stored in your google account. These settings differ from
// regular prefs, since they originate from the user's account and are available
// beyond Chrome.
class AccountSettingService : public KeyedService {
 public:
  AccountSettingService() = default;
  ~AccountSettingService() override = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
