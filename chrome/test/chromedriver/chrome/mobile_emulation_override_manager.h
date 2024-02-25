// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"

class DevToolsClient;
class Status;

// Overrides the device metrics, if requested, for the duration of the
// given |DevToolsClient|'s lifetime.
class MobileEmulationOverrideManager : public DevToolsEventListener {
 public:
  MobileEmulationOverrideManager(DevToolsClient* client,
                                 std::optional<MobileDevice> mobile_device,
                                 int browser_major_version);

  MobileEmulationOverrideManager(const MobileEmulationOverrideManager&) =
      delete;
  MobileEmulationOverrideManager& operator=(
      const MobileEmulationOverrideManager&) = delete;

  ~MobileEmulationOverrideManager() override;

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

  bool IsEmulatingTouch() const;
  bool HasOverrideMetrics() const;
  Status RestoreOverrideMetrics();
  const DeviceMetrics* GetDeviceMetrics() const;

 private:
  Status ApplyOverrideIfNeeded();

  raw_ptr<DevToolsClient> client_;
  std::optional<MobileDevice> mobile_device_;
  int browser_major_version_ = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_EMULATION_OVERRIDE_MANAGER_H_
