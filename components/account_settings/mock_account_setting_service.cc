// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_settings/mock_account_setting_service.h"

namespace account_settings {

MockAccountSettingService::MockAccountSettingService() {
  ON_CALL(*this, IsLoaded).WillByDefault(testing::Return(true));
}

MockAccountSettingService::~MockAccountSettingService() = default;

}  // namespace account_settings
