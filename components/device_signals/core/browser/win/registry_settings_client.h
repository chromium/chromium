// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_REGISTRY_SETTINGS_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_REGISTRY_SETTINGS_CLIENT_H_

#include "components/device_signals/core/browser/settings_client.h"

namespace device_signals {

// Settings Client that collects signals from Windows Registry.
// This will be used by SettingsSignalsCollector on Windows.
class RegistrySettingsClient : public SettingsClient {
 public:
  RegistrySettingsClient();
  ~RegistrySettingsClient() override;

  // SettingsClient:
  void GetSettings(const std::vector<GetSettingsOptions>& options,
                   GetSettingsSignalsCallback callback) override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_REGISTRY_SETTINGS_CLIENT_H_
