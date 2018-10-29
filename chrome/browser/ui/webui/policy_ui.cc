// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreatePolicyUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);
  source->AddLocalizedString("filterPlaceholder",
                             IDS_POLICY_FILTER_PLACEHOLDER);
  source->AddLocalizedString("reloadPolicies", IDS_POLICY_RELOAD_POLICIES);
  source->AddLocalizedString("exportPoliciesJSON", IDS_EXPORT_POLICIES_JSON);
  source->AddLocalizedString("status", IDS_POLICY_STATUS);
  source->AddLocalizedString("statusDevice", IDS_POLICY_STATUS_DEVICE);
  source->AddLocalizedString("statusUser", IDS_POLICY_STATUS_USER);
  source->AddLocalizedString("statusMachine", IDS_POLICY_STATUS_MACHINE);
  source->AddLocalizedString("labelEnterpriseEnrollmentDomain",
                             IDS_POLICY_LABEL_ENTERPRISE_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("labelEnterpriseDisplayDomain",
                             IDS_POLICY_LABEL_ENTERPRISE_DISPLAY_DOMAIN);
  source->AddLocalizedString("labelMachineEnrollmentDomain",
                             IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("labelMachineEnrollmentToken",
                             IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN);
  source->AddLocalizedString("labelMachineEntrollmentDeviceId",
                             IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DEVICE_ID);
  source->AddLocalizedString("labelMachineEnrollmentMachineName",
                             IDS_POLICY_LABEL_MACHINE_ENROLLMENT_MACHINE_NAME);
  source->AddLocalizedString("labelUsername", IDS_POLICY_LABEL_USERNAME);
  source->AddLocalizedString("labelGaiaId", IDS_POLICY_LABEL_GAIA_ID);
  source->AddLocalizedString("labelClientId", IDS_POLICY_LABEL_CLIENT_ID);
  source->AddLocalizedString("labelAssetId", IDS_POLICY_LABEL_ASSET_ID);
  source->AddLocalizedString("labelLocation", IDS_POLICY_LABEL_LOCATION);
  source->AddLocalizedString("labelDirectoryApiId",
                             IDS_POLICY_LABEL_DIRECTORY_API_ID);
  source->AddLocalizedString("labelTimeSinceLastRefresh",
                             IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH);
  source->AddLocalizedString("labelRefreshInterval",
                             IDS_POLICY_LABEL_REFRESH_INTERVAL);
  source->AddLocalizedString("labelStatus", IDS_POLICY_LABEL_STATUS);
  source->AddLocalizedString("showUnset", IDS_POLICY_SHOW_UNSET);
  source->AddLocalizedString("noPoliciesSet", IDS_POLICY_NO_POLICIES_SET);
  source->AddLocalizedString("showExpandedValue",
                             IDS_POLICY_SHOW_EXPANDED_VALUE);
  source->AddLocalizedString("hideExpandedValue",
                             IDS_POLICY_HIDE_EXPANDED_VALUE);
  source->AddLocalizedString("policyLearnMore", IDS_POLICY_LEARN_MORE);
  // Add required resources.
#if !defined(OS_ANDROID)
  source->AddResourcePath("policy_common.css", IDR_POLICY_COMMON_CSS);
#endif
  source->AddResourcePath("policy.css", IDR_POLICY_CSS);
  source->AddResourcePath("policy_base.js", IDR_POLICY_BASE_JS);
  source->AddResourcePath("policy.js", IDR_POLICY_JS);
  source->SetDefaultResource(IDR_POLICY_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PolicyUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePolicyUIHtmlSource());
}

PolicyUI::~PolicyUI() {
}
