// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/notification_tester/notification_tester_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

NotificationTesterUI::NotificationTesterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://notification-tester source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUINotificationTesterHost);

  // Add required resources.
  html_source->SetDefaultResource(IDR_NOTIFICATION_TESTER_HTML);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

NotificationTesterUI::~NotificationTesterUI() = default;

}  // namespace chromeos
