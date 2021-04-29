// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

namespace base {
class DictionaryValue;
}

class DevToolsClient;
struct DeviceMetrics;
class Status;

// Overrides the device metrics, if requested, for the duration of the
// given |DevToolsClient|'s lifetime.
class MobileEmulationOverrideManager : public DevToolsEventListener {
 public:
  MobileEmulationOverrideManager(DevToolsClient* client,
                                 const DeviceMetrics* device_metrics);
  ~MobileEmulationOverrideManager() override;

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

  bool IsEmulatingTouch() const;
  bool HasOverrideMetrics() const;
  Status RestoreOverrideMetrics();
  const DeviceMetrics* GetDeviceMetrics() const;

 private:
  Status ApplyOverrideIfNeeded();

  DevToolsClient* client_;
  const DeviceMetrics* overridden_device_metrics_;

  DISALLOW_COPY_AND_ASSIGN(MobileEmulationOverrideManager);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_
