// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"

#include <utility>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

namespace {

constexpr char kOptInVisibility[] = "optInVisibility";
constexpr char kOptInDefaultState[] = "optInDefaultState";
constexpr char kLegalFooterVisibility[] = "legalFooterVisibility";
constexpr char kCloudGamingDevice[] = "cloudGamingDevice";

}  // namespace

MarketingOptInScreenHandler::MarketingOptInScreenHandler()
    : BaseScreenHandler(kScreenId) {}

MarketingOptInScreenHandler::~MarketingOptInScreenHandler() = default;

void MarketingOptInScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("marketingOptInScreenTitle",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_TITLE);
  builder->Add("marketingOptInScreenGameDeviceTitle",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_WITH_CLOUDGAMINGDEVICE_TITLE);
  builder->AddF(
      "marketingOptInScreenGameDeviceSubtitle",
      IDS_LOGIN_MARKETING_OPT_IN_SCREEN_WITH_CLOUDGAMINGDEVICE_SUBTITLE,
      ui::GetChromeOSDeviceName());
  builder->AddF("marketingOptInScreenSubtitle",
                IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SUBTITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("marketingOptInScreenSubtitleWithDeviceName",
                IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SUBTITLE_WITH_DEVICE_NAME,
                ui::GetChromeOSDeviceName());
  builder->Add(
      "marketingOptInGetChromebookUpdates",
      IDS_LOGIN_MARKETING_OPT_IN_SCREEN_GET_CHROMEBOOK_UPDATES_SIGN_ME_UP);
  builder->AddF(
      "marketingOptInGameDeviceUpdates",
      IDS_LOGIN_MARKETING_OPT_IN_SCREEN_WITH_CLOUDGAMINGDEVICE_SIGN_ME_UP,
      ui::GetChromeOSDeviceName());
  builder->Add("marketingOptInScreenAllSet", IDS_LOGIN_GET_STARTED);
  builder->Add("marketingOptInScreenUnsubscribeShort",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_UNSUBSCRIBE_SHORT);
  builder->Add("marketingOptInScreenUnsubscribeLong",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_UNSUBSCRIBE_LONG);
  builder->Add("marketingOptInA11yButtonLabel",
               IDS_MARKETING_OPT_IN_ACCESSIBILITY_BUTTON_LABEL);
  builder->Add("finalA11yPageTitle", IDS_MARKETING_OPT_IN_ACCESSIBILITY_TITLE);
  builder->Add("finalA11yPageNavButtonSettingTitle",
               IDS_MARKETING_OPT_IN_ACCESSIBILITY_NAV_BUTTON_SETTING_TITLE);
  builder->Add(
      "finalA11yPageNavButtonSettingDescription",
      IDS_MARKETING_OPT_IN_ACCESSIBILITY_NAV_BUTTON_SETTING_DESCRIPTION);
  builder->Add("finalA11yPageDoneButtonTitle",
               IDS_MARKETING_OPT_IN_ACCESSIBILITY_DONE_BUTTON);
}

void MarketingOptInScreenHandler::Show(bool opt_in_visible,
                                       bool opt_in_default_state,
                                       bool legal_footer_visible,
                                       bool cloud_gaming_enabled) {
  base::Value::Dict data;
  data.Set(kOptInVisibility, opt_in_visible);
  data.Set(kOptInDefaultState, opt_in_default_state);
  data.Set(kLegalFooterVisibility, legal_footer_visible);
  data.Set(kCloudGamingDevice, cloud_gaming_enabled);

  ShowInWebUI(std::move(data));
}

void MarketingOptInScreenHandler::UpdateA11ySettingsButtonVisibility(
    bool shown) {
  CallExternalAPI("updateA11ySettingsButtonVisibility", shown);
}

void MarketingOptInScreenHandler::UpdateA11yShelfNavigationButtonToggle(
    bool enabled) {
  CallExternalAPI("updateA11yNavigationButtonToggle", enabled);
}

base::WeakPtr<MarketingOptInScreenView>
MarketingOptInScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void MarketingOptInScreenHandler::GetAdditionalParameters(
    base::Value::Dict* parameters) {
  BaseScreenHandler::GetAdditionalParameters(parameters);
}

}  // namespace ash
