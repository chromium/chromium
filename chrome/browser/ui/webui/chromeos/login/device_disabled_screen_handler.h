// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class DeviceDisabledScreen;

// Interface between the device disabled screen and its representation.
class DeviceDisabledScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"device-disabled"};

  virtual ~DeviceDisabledScreenView() {}

  virtual void Show(const std::string& serial,
                    const std::string& domain,
                    const std::string& message) = 0;
  virtual void Hide() = 0;
  virtual void Bind(DeviceDisabledScreen* screen) = 0;
  virtual void UpdateMessage(const std::string& message) = 0;
};

// WebUI implementation of DeviceDisabledScreenActor.
class DeviceDisabledScreenHandler : public DeviceDisabledScreenView,
                                    public BaseScreenHandler {
 public:
  using TView = DeviceDisabledScreenView;

  explicit DeviceDisabledScreenHandler(JSCallsContainer* js_calls_container);
  ~DeviceDisabledScreenHandler() override;

  // DeviceDisabledScreenActor:
  void Show(const std::string& serial,
            const std::string& domain,
            const std::string& message) override;
  void Hide() override;
  void Bind(DeviceDisabledScreen* screen) override;
  void UpdateMessage(const std::string& message) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  DeviceDisabledScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisabledScreenHandler);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::DeviceDisabledScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

