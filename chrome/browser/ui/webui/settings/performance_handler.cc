// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/performance_manager/public/features.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using content::WebContents;

namespace settings {

PerformanceHandler::PerformanceHandler() = default;
PerformanceHandler::~PerformanceHandler() = default;

void PerformanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceHasBattery",
      base::BindRepeating(&PerformanceHandler::HandleGetDeviceHasBattery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openPerformanceFeedbackDialog",
      base::BindRepeating(&PerformanceHandler::HandleOpenFeedbackDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validateTabDiscardExceptionRule",
      base::BindRepeating(
          &PerformanceHandler::HandleValidateTabDiscardExceptionRule,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCurrentOpenSites",
      base::BindRepeating(&PerformanceHandler::HandleGetCurrentOpenSites,
                          base::Unretained(this)));
}

void PerformanceHandler::OnJavascriptAllowed() {
  performance_handler_observer_.Observe(
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance());
}

void PerformanceHandler::OnJavascriptDisallowed() {
  performance_handler_observer_.Reset();
}

void PerformanceHandler::OnDeviceHasBatteryChanged(bool device_has_battery) {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("device-has-battery-changed", device_has_battery);
}

base::Value PerformanceHandler::GetCurrentOpenSites() {
  base::Value::List hosts;
  std::set<std::pair<base::TimeTicks, std::string>, std::greater<>>
      last_active_time_host_pairs;
  const Profile* profile = Profile::FromWebUI(web_ui());
  for (Browser* browser : *BrowserList::GetInstance()) {
    // Exclude browsers not signed into the current profile
    if (browser->profile() != profile) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();

    for (int tab_index = 0; tab_index < tab_strip_model->count(); ++tab_index) {
      WebContents* web_contents = tab_strip_model->GetWebContentsAt(tab_index);
      const GURL url = web_contents->GetLastCommittedURL();
      if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
        last_active_time_host_pairs.insert(
            std::make_pair(web_contents->GetLastActiveTimeTicks(), url.host()));
      }
    }
  }

  std::unordered_set<std::string> added_hosts;
  for (auto& [last_active_time, host] : last_active_time_host_pairs) {
    if (!base::Contains(added_hosts, host)) {
      added_hosts.insert(host);
      hosts.Append(host);
    }
  }

  return base::Value(std::move(hosts));
}

void PerformanceHandler::HandleGetCurrentOpenSites(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, GetCurrentOpenSites());
}

void PerformanceHandler::HandleGetDeviceHasBattery(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id, base::Value(performance_manager::user_tuning::
                                   BatterySaverModeManager::GetInstance()
                                       ->DeviceHasBattery()));
}

void PerformanceHandler::HandleOpenFeedbackDialog(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string category_tag = args[0].GetString();

  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  DCHECK(browser);
  std::string unused;
  chrome::ShowFeedbackPage(browser,
                           feedback::kFeedbackSourceSettingsPerformancePage,
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
