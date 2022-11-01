// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/performance_manager/public/features.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/web_ui.h"

namespace settings {

PerformanceHandler::PerformanceHandler() = default;
PerformanceHandler::~PerformanceHandler() = default;

void PerformanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceHasBattery",
      base::BindRepeating(&PerformanceHandler::HandleGetDeviceHasBattery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openBatterySaverFeedbackDialog",
      base::BindRepeating(
          &PerformanceHandler::HandleOpenBatterySaverFeedbackDialog,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openHighEfficiencyFeedbackDialog",
      base::BindRepeating(
          &PerformanceHandler::HandleOpenHighEfficiencyFeedbackDialog,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validateTabDiscardExceptionRule",
      base::BindRepeating(
          &PerformanceHandler::HandleValidateTabDiscardExceptionRule,
          base::Unretained(this)));
}

void PerformanceHandler::OnJavascriptAllowed() {
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable)) {
    performance_handler_observer_.Observe(
        performance_manager::user_tuning::UserPerformanceTuningManager::
            GetInstance());
  }
}

void PerformanceHandler::OnJavascriptDisallowed() {
  performance_handler_observer_.Reset();
}

void PerformanceHandler::OnDeviceHasBatteryChanged(bool device_has_battery) {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("device-has-battery-changed", device_has_battery);
}

void PerformanceHandler::HandleGetDeviceHasBattery(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id, base::Value(performance_manager::user_tuning::
                                   UserPerformanceTuningManager::GetInstance()
                                       ->DeviceHasBattery()));
}

void PerformanceHandler::HandleOpenBatterySaverFeedbackDialog(
    const base::Value::List& args) {
  HandleOpenFeedbackDialog("performance_battery");
}

void PerformanceHandler::HandleOpenHighEfficiencyFeedbackDialog(
    const base::Value::List& args) {
  HandleOpenFeedbackDialog("performance_tabs");
}

void PerformanceHandler::HandleOpenFeedbackDialog(
    const std::string category_tag) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  DCHECK(browser);
  std::string unused;
  chrome::ShowFeedbackPage(browser,
                           chrome::kFeedbackSourceSettingsPerformancePage,
                           unused, unused, category_tag, unused);
}

void PerformanceHandler::HandleValidateTabDiscardExceptionRule(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string rule = args[1].GetString();

  AllowJavascript();

  url_matcher::util::FilterComponents components;

  bool is_valid = url_matcher::util::FilterToComponents(
      rule, &components.scheme, &components.host, &components.match_subdomains,
      &components.port, &components.path, &components.query);

  ResolveJavascriptCallback(callback_id, base::Value(is_valid));
}

}  // namespace settings
