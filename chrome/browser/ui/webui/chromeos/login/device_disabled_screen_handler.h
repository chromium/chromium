// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface between the device disabled screen and its representation.
class DeviceDisabledScreenView
    : public base::SupportsWeakPtr<DeviceDisabledScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"device-disabled",
                                                       "DeviceDisabledScreen"};

  virtual ~DeviceDisabledScreenView() = default;

  virtual void Show(const std::string& serial,
                    const std::string& domain,
                    const std::string& message) = 0;
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
            const std::string& message) override;
  void UpdateMessage(const std::string& message) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::DeviceDisabledScreenHandler;
using ::chromeos::DeviceDisabledScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
