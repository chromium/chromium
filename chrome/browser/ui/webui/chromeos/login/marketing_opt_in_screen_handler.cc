// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

constexpr char kOptInVisibility[] = "optInVisibility";
constexpr char kOptInDefaultState[] = "optInDefaultState";
constexpr char kLegalFooterVisibility[] = "legalFooterVisibility";

void RecordShowShelfNavigationButtonsValueChange(bool enabled) {
  base::UmaHistogramBoolean(
      "Accessibility.CrosShelfNavigationButtonsInTabletModeChanged.OOBE",
      enabled);
}

}  // namespace

constexpr StaticOobeScreenId MarketingOptInScreenView::kScreenId;

MarketingOptInScreenHandler::MarketingOptInScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

MarketingOptInScreenHandler::~MarketingOptInScreenHandler() {
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning())
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
}

void MarketingOptInScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("marketingOptInScreenTitle",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_TITLE);
  builder->AddF("marketingOptInScreenSubtitle",
                IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SUBTITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("marketingOptInScreenSubtitleWithDeviceName",
                IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SUBTITLE_WITH_DEVICE_NAME,
                ui::GetChromeOSDeviceName());
  builder->Add(
      "marketingOptInGetChromebookUpdates",
      IDS_LOGIN_MARKETING_OPT_IN_SCREEN_GET_CHROMEBOOK_UPDATES_SIGN_ME_UP);
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

void MarketingOptInScreenHandler::Bind(MarketingOptInScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void MarketingOptInScreenHandler::Show(bool opt_in_visible,
                                       bool opt_in_default_state,
                                       bool legal_footer_visible) {
  base::DictionaryValue data;
  data.SetBoolean(kOptInVisibility, opt_in_visible);
  data.SetBoolean(kOptInDefaultState, opt_in_default_state);
  data.SetBoolean(kLegalFooterVisibility, legal_footer_visible);

  ShowScreenWithData(kScreenId, &data);
}

void MarketingOptInScreenHandler::Hide() {
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning())
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
}

void MarketingOptInScreenHandler::UpdateA11ySettingsButtonVisibility(
    bool shown) {
  CallJS("login.MarketingOptInScreen.updateA11ySettingsButtonVisibility",
         shown);
}

void MarketingOptInScreenHandler::UpdateA11yShelfNavigationButtonToggle(
    bool enabled) {
  CallJS("login.MarketingOptInScreen.updateA11yNavigationButtonToggle",
         enabled);
}

void MarketingOptInScreenHandler::Initialize() {}

void MarketingOptInScreenHandler::RegisterMessages() {
  AddCallback("login.MarketingOptInScreen.onGetStarted",
              &MarketingOptInScreenHandler::HandleOnGetStarted);
  AddCallback(
      "login.MarketingOptInScreen.setA11yNavigationButtonsEnabled",
      &MarketingOptInScreenHandler::HandleSetA11yNavigationButtonsEnabled);
}

void MarketingOptInScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* parameters) {
  BaseScreenHandler::GetAdditionalParameters(parameters);
}

void MarketingOptInScreenHandler::HandleOnGetStarted(
    bool chromebook_email_opt_in) {
  screen_->OnGetStarted(chromebook_email_opt_in);
}

void MarketingOptInScreenHandler::HandleSetA11yNavigationButtonsEnabled(
    bool enabled) {
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled,
      enabled);
  a11y_nav_buttons_toggle_metrics_reporter_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(10),
      base::BindOnce(&RecordShowShelfNavigationButtonsValueChange, enabled));
}

}  // namespace chromeos
