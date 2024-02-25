// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SETTINGS_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SETTINGS_CLIENT_H_

#include "base/functional/callback.h"
#include "components/device_signals/core/browser/settings_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockSettingsClient : public SettingsClient {
 public:
  MockSettingsClient();
  ~MockSettingsClient() override;
  MOCK_METHOD(void,
              GetSettings,
              (const std::vector<GetSettingsOptions>& options,
               GetSettingsSignalsCallback callback),
              (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SETTINGS_CLIENT_H_