// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreateManagementUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);
  source->AddLocalizedString("title", IDS_MANAGEMENT_TITLE);
  source->AddLocalizedString("deviceReporting",
                             IDS_MANAGEMENT_DEVICE_REPORTING);
  source->AddLocalizedString("deviceConfiguration",
                             IDS_MANAGEMENT_DEVICE_CONFIGURATION);
  source->AddLocalizedString("extensionReporting",
                             IDS_MANAGEMENT_EXTENSION_REPORTING);
  source->AddLocalizedString("extensionsInstalled",
                             IDS_MANAGEMENT_EXTENSIONS_INSTALLED);
  source->AddLocalizedString("extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME);
  source->AddLocalizedString("extensionPermissions",
                             IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS);
  source->AddLocalizedString(kManagementLogUploadEnabled,
                             IDS_MANAGEMENT_LOG_UPLOAD_ENABLED);
  source->AddLocalizedString(kManagementReportActivityTimes,
                             IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES);
  source->AddLocalizedString(kManagementReportHardwareStatus,
                             IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_STATUS);
  source->AddLocalizedString(kManagementReportNetworkInterfaces,
                             IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_INTERFACES);
  source->AddLocalizedString(kManagementReportUsers,
                             IDS_MANAGEMENT_REPORT_DEVICE_USERS);
  source->SetJsonPath("strings.js");
  // Add required resources.
  source->AddResourcePath("management.css", IDR_MANAGEMENT_CSS);
  source->AddResourcePath("management.js", IDR_MANAGEMENT_JS);
  source->SetDefaultResource(IDR_MANAGEMENT_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ManagementUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateManagementUIHtmlSource());
}

ManagementUI::~ManagementUI() {}
