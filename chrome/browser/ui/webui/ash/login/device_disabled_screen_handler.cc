// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DeviceDisabledScreenView::Params::Params() = default;
DeviceDisabledScreenView::Params::~Params() = default;

DeviceDisabledScreenHandler::DeviceDisabledScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DeviceDisabledScreenHandler::~DeviceDisabledScreenHandler() = default;

void DeviceDisabledScreenHandler::Show(const Params& params) {
  ShowInWebUI(
      base::Value::Dict()
          .Set("serial", params.serial)
          .Set("domain", params.domain)
          .Set("message", params.message)
          .Set("deviceRestrictionScheduleEnabled",
               params.device_restriction_schedule_enabled)
          .Set("deviceName", params.device_name)
          .Set("restrictionScheduleEndDay", params.restriction_schedule_end_day)
          .Set("restrictionScheduleEndTime",
               params.restriction_schedule_end_time));
}

void DeviceDisabledScreenHandler::UpdateMessage(const std::string& message) {
  CallExternalAPI("updateData", base::Value::Dict().Set("message", message));
}

void DeviceDisabledScreenHandler::UpdateRestrictionScheduleMessage(
    const std::u16string& end_day,
    const std::u16string& end_time) {
  CallExternalAPI("updateData",
                  base::Value::Dict()
                      .Set("restrictionScheduleEndDay", end_day)
                      .Set("restrictionScheduleEndTime", end_time));
}

base::WeakPtr<DeviceDisabledScreenView>
DeviceDisabledScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeviceDisabledScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceDisabledHeading", IDS_DEVICE_DISABLED_HEADING);
  builder->Add("deviceDisabledExplanationWithDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITH_DOMAIN);
  builder->Add("deviceDisabledExplanationWithoutDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITHOUT_DOMAIN);
  builder->Add("deviceDisabledHeadingRestrictionSchedule",
               IDS_DEVICE_DISABLED_HEADING_RESTRICTION_SCHEDULE);
  builder->Add("deviceDisabledExplanationRestrictionSchedule",
               IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE);
  builder->Add("deviceDisabledExplanationRestrictionScheduleTime",
               IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TIME);
}

}  // namespace ash
