// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

namespace phonehub {
class FakePhoneHubManager;
}  // namespace phonehub

namespace multidevice {

// WebUIMessageHandler for chrome://multidevice-internals PhoneHub section.
class MultidevicePhoneHubHandler : public content::WebUIMessageHandler {
 public:
  MultidevicePhoneHubHandler();
  MultidevicePhoneHubHandler(const MultidevicePhoneHubHandler&) = delete;
  MultidevicePhoneHubHandler& operator=(const MultidevicePhoneHubHandler&) =
      delete;
  ~MultidevicePhoneHubHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void SetSystemPhoneHubManagerEnabled();
  void SetFakePhoneHubManagerEnabled();
  void HandleSetFakePhoneHubManagerEnabled(const base::ListValue* args);
  void HandleSetFeatureStatus(const base::ListValue* args);

  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
};

}  // namespace multidevice
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_
