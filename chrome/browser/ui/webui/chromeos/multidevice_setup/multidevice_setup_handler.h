// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

namespace multidevice_setup {

// Chrome MultiDevice setup flow WebUI handler.
class MultideviceSetupHandler : public content::WebUIMessageHandler {
 public:
  MultideviceSetupHandler();

  MultideviceSetupHandler(const MultideviceSetupHandler&) = delete;
  MultideviceSetupHandler& operator=(const MultideviceSetupHandler&) = delete;

  ~MultideviceSetupHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleGetProfileInfo(const base::ListValue* args);
  void HandleOpenMultiDeviceSettings(const base::ListValue* args);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_
