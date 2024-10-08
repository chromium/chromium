// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between the device disabled screen and its representation.
class DeviceDisabledScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"device-disabled",
                                                       "DeviceDisabledScreen"};

  virtual ~DeviceDisabledScreenView() = default;

  // Sets some properties of the `DeviceDisabledScreen`, then shows the screen,
  // by calling `ShowInWebUI()`. Receives the following data, respectively:
  // serial number of the device, domain that owns the device (can be empty),
  // message from the admin, flag indicating if the device was disabled
  // because the device is in restriction schedule, device name, and the day and
  // time at which the restriction schedule ends.
  struct Params {
    Params();
    ~Params();
    Params(const Params&) = delete;
    Params& operator=(Params&) = delete;
    std::string serial;
    std::string domain;
    std::string message;
    bool device_restriction_schedule_enabled;
    std::u16string device_name;
    std::u16string restriction_schedule_end_day;
    std::u16string restriction_schedule_end_time;
  };
  virtual void Show(const Params& params) = 0;

  virtual void UpdateMessage(const std::string& message) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<DeviceDisabledScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of DeviceDisabledScreenActor.
class DeviceDisabledScreenHandler final : public DeviceDisabledScreenView,
                                          public BaseScreenHandler {
 public:
  using TView = DeviceDisabledScreenView;

  DeviceDisabledScreenHandler();

  DeviceDisabledScreenHandler(const DeviceDisabledScreenHandler&) = delete;
  DeviceDisabledScreenHandler& operator=(const DeviceDisabledScreenHandler&) =
      delete;

  ~DeviceDisabledScreenHandler() override;

  // DeviceDisabledScreenView:
  void Show(const Params& params) override;
  void UpdateMessage(const std::string& message) override;
  base::WeakPtr<DeviceDisabledScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<DeviceDisabledScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
