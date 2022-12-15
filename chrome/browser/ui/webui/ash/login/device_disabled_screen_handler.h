// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between the device disabled screen and its representation.
class DeviceDisabledScreenView
    : public base::SupportsWeakPtr<DeviceDisabledScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"device-disabled",
                                                       "DeviceDisabledScreen"};

  virtual ~DeviceDisabledScreenView() = default;

  // Sets some properties of the `DeviceDisabledScreen`, then shows the screen,
  // by calling `ShowInWebUI()`. Receives the following data, respectively:
  // serial number of the device, domain that owns the device (can be empty),
  // message from the admin, and a flag indicating if the device was disabled
  // because it's in AD mode (which is no longer supported).
  virtual void Show(const std::string& serial,
                    const std::string& domain,
                    const std::string& message,
                    bool is_disabled_ad_device) = 0;
  virtual void UpdateMessage(const std::string& message) = 0;
};

// WebUI implementation of DeviceDisabledScreenActor.
class DeviceDisabledScreenHandler : public DeviceDisabledScreenView,
                                    public BaseScreenHandler {
 public:
  using TView = DeviceDisabledScreenView;

  DeviceDisabledScreenHandler();

  DeviceDisabledScreenHandler(const DeviceDisabledScreenHandler&) = delete;
  DeviceDisabledScreenHandler& operator=(const DeviceDisabledScreenHandler&) =
      delete;

  ~DeviceDisabledScreenHandler() override;

  // DeviceDisabledScreenView:
  void Show(const std::string& serial,
            const std::string& domain,
            const std::string& message,
            bool is_disabled_ad_device) override;
  void UpdateMessage(const std::string& message) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
