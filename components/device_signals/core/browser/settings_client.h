// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_CLIENT_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/device_signals/core/browser/signals_types.h"

namespace device_signals {

// Interface that collects settings signals.
class SettingsClient {
 public:
  virtual ~SettingsClient() = default;

  using GetSettingsSignalsCallback = base::OnceCallback<void(
      const std::vector<::device_signals::SettingsItem>&)>;

  // Function for collecting settings signals, returns a vector of settings
  // presence or settings values through callback
  virtual void GetSettings(const std::vector<GetSettingsOptions>& requests,
                           GetSettingsSignalsCallback callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_CLIENT_H_
