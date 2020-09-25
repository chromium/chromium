// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace cellular_setup {

namespace {

constexpr webui::LocalizedString kLocalizedStringsWithoutPlaceholders[] = {
    {"activationCode", IDS_CELLULAR_SETUP_ESIM_PAGE_ACTIVATION_CODE},
    {"cancel", IDS_CANCEL},
    {"back", IDS_CELLULAR_SETUP_BACK_LABEL},
    {"done", IDS_CELLULAR_SETUP_DONE_LABEL},
    {"eSimFlowSetup", IDS_CELLULAR_SETUP_CELLULAR_SETUP_PAGE_ESIM_LABEL},
    {"eSimFlowSetupMessage",
     IDS_CELLULAR_SETUP_CELLULAR_SETUP_PAGE_ESIM_MESSAGE},
    {"establishNetworkConnectionMessage",
     IDS_CELLULAR_SETUP_ESTABLISH_NETWORK_CONNECTION},
    {"next", IDS_CELLULAR_SETUP_NEXT_LABEL},
    {"tryAgain", IDS_CELLULAR_SETUP_TRY_AGAIN_LABEL},
    {"simDetectPageTitle", IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_TITLE},
    {"simDetectPageErrorTitle", IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_ERROR_TITLE},
    {"simDetectPageErrorMessage",
     IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_ERROR_MESSAGE},
    {"provisioningPageLoadingTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_LOADING_TITLE},
    {"provisioningPageActiveTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ACTIVE_TITLE},
    {"provisioningPageErrorTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ERROR_TITLE},
    {"provisioningPageErrorMessage",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ERROR_MESSAGE},
    {"pSimFlowSetup", IDS_CELLULAR_SETUP_CELLULAR_SETUP_PAGE_PSIM_LABEL},
    {"pSimFlowSetupMessage",
     IDS_CELLULAR_SETUP_CELLULAR_SETUP_PAGE_PSIM_MESSAGE},
    {"finalPageTitle", IDS_CELLULAR_SETUP_FINAL_PAGE_TITLE},
    {"finalPageMessage", IDS_CELLULAR_SETUP_FINAL_PAGE_MESSAGE},
    {"finalPageErrorTitle", IDS_CELLULAR_SETUP_FINAL_PAGE_ERROR_TITLE},
    {"finalPageErrorMessage", IDS_CELLULAR_SETUP_FINAL_PAGE_ERROR_MESSAGE},
    {"scanQRCode", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE}};
}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddLocalizedStringsBulk(html_source, kLocalizedStringsWithoutPlaceholders);
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : kLocalizedStringsWithoutPlaceholders)
    builder->Add(entry.name, entry.id);
}

}  // namespace cellular_setup

}  // namespace chromeos
