// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_ui.h"

#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/notification_tester_resources.h"
#include "chrome/grit/notification_tester_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

NotificationTesterUI::NotificationTesterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://notification-tester source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUINotificationTesterHost);

  // Add required resources.
  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kNotificationTesterResources,
                                              kNotificationTesterResourcesSize),
                              IDR_NOTIFICATION_TESTER_INDEX_HTML);

  // Add message handler.
  web_ui->AddMessageHandler(std::make_unique<NotificationTesterHandler>());
}

NotificationTesterUI::~NotificationTesterUI() = default;

}  // namespace ash
