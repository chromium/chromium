// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/device_disabled_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId DeviceDisabledScreenView::kScreenId;

DeviceDisabledScreenHandler::DeviceDisabledScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

DeviceDisabledScreenHandler::~DeviceDisabledScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void DeviceDisabledScreenHandler::Show(const std::string& serial,
                                       const std::string& domain,
                                       const std::string& message) {
  base::DictionaryValue screen_data;
  screen_data.SetStringPath("serial", serial);
  screen_data.SetStringPath("domain", domain);
  screen_data.SetStringPath("message", message);
  ShowScreenWithData(kScreenId, &screen_data);
}

void DeviceDisabledScreenHandler::Hide() {
  NOTREACHED() << "Device should reboot upon removing device disabled flag";
}

void DeviceDisabledScreenHandler::Bind(DeviceDisabledScreen* screen) {
  screen_ = screen;
}

void DeviceDisabledScreenHandler::UpdateMessage(const std::string& message) {
  CallJS("login.DeviceDisabledScreen.setMessage", message);
}

void DeviceDisabledScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceDisabledHeading", IDS_DEVICE_DISABLED_HEADING);
  builder->Add("deviceDisabledExplanationWithDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITH_DOMAIN);
  builder->Add("deviceDisabledExplanationWithoutDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITHOUT_DOMAIN);
}

void DeviceDisabledScreenHandler::Initialize() {
}

void DeviceDisabledScreenHandler::RegisterMessages() {
}

}  // namespace chromeos
