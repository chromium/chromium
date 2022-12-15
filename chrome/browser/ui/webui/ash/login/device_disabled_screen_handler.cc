// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DeviceDisabledScreenHandler::DeviceDisabledScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DeviceDisabledScreenHandler::~DeviceDisabledScreenHandler() = default;

void DeviceDisabledScreenHandler::Show(const std::string& serial,
                                       const std::string& domain,
                                       const std::string& message,
                                       bool is_disabled_ad_device) {
  base::Value::Dict screen_data;
  screen_data.Set("serial", serial);
  screen_data.Set("domain", domain);
  screen_data.Set("message", message);
  screen_data.Set("isDisabledAdDevice", is_disabled_ad_device);
  ShowInWebUI(std::move(screen_data));
}

void DeviceDisabledScreenHandler::UpdateMessage(const std::string& message) {
  CallExternalAPI("setMessage", message);
}

void DeviceDisabledScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceDisabledHeading", IDS_DEVICE_DISABLED_HEADING);
  builder->Add("deviceDisabledExplanationWithDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITH_DOMAIN);
  builder->Add("deviceDisabledExplanationWithoutDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITHOUT_DOMAIN);
  builder->Add("deviceDisabledAdModeExplanation",
               IDS_DEVICE_DISABLED_CHROMAD_DISABLED);
}

}  // namespace ash
