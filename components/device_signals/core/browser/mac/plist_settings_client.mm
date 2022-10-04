// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/mac/plist_settings_client.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "components/device_signals/core/browser/signals_types.h"

namespace device_signals {

PlistSettingsClient::PlistSettingsClient() = default;

PlistSettingsClient::~PlistSettingsClient() = default;

void PlistSettingsClient::GetSettings(
    const std::vector<GetSettingsOptions>& options,
    GetSettingsSignalsCallback callback) {
  // TODO(b:245524787): Implement.
  std::move(callback).Run(std::vector<SettingsItem>());
}

}  // namespace device_signals
