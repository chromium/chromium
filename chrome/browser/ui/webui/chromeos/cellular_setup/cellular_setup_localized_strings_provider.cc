// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace cellular_setup {
namespace {

const char useExternalEuiccLoadTimeDataName[] = "useExternalEuicc";

constexpr webui::LocalizedString kLocalizedStringsWithoutPlaceholders[] = {
    {"activationCode", IDS_CELLULAR_SETUP_ESIM_PAGE_ACTIVATION_CODE},
    {"cancel", IDS_CANCEL},
    {"back", IDS_CELLULAR_SETUP_BACK_LABEL},
    {"done", IDS_CELLULAR_SETUP_DONE_LABEL},
    {"establishNetworkConnectionMessage",
     IDS_CELLULAR_SETUP_ESTABLISH_NETWORK_CONNECTION},
    {"next", IDS_CELLULAR_SETUP_NEXT_LABEL},
    {"tryAgain", IDS_CELLULAR_SETUP_TRY_AGAIN_LABEL},
    {"skipDiscovery", IDS_CELLULAR_SETUP_SKIP_DISCOVERY_LABEL},
    {"confirm", IDS_CELLULAR_SETUP_CONFIRM_LABEL},
    {"simDetectPageTitle", IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_TITLE},
    {"simDetectPageErrorTitle", IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_ERROR_TITLE},
    {"simDetectPageErrorMessage",
     IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_ERROR_MESSAGE},
    {"simDetectPageFinalErrorMessage",
     IDS_CELLULAR_SETUP_SIM_DETECT_PAGE_FINAL_ERROR_MESSAGE},
    {"provisioningPageLoadingTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_LOADING_TITLE},
    {"provisioningPageActiveTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ACTIVE_TITLE},
    {"provisioningPageErrorTitle",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ERROR_TITLE},
    {"provisioningPageErrorMessage",
     IDS_CELLULAR_SETUP_PROVISIONING_PAGE_ERROR_MESSAGE},
    {"finalPageTitle", IDS_CELLULAR_SETUP_FINAL_PAGE_TITLE},
    {"pSimfinalPageMessage", IDS_CELLULAR_SETUP_PSIM_FINAL_PAGE_MESSAGE},
    {"finalPageErrorTitle", IDS_CELLULAR_SETUP_FINAL_PAGE_ERROR_TITLE},
    {"finalPageErrorMessage", IDS_CELLULAR_SETUP_FINAL_PAGE_ERROR_MESSAGE},
    {"eSimFinalPageMessage", IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_MESSAGE},
    {"eSimFinalPageErrorMessage",
     IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_ERROR_MESSAGE},
    {"eSimProfileDetectMessage",
     IDS_CELLULAR_SETUP_ESIM_PROFILE_DETECT_MESSAGE},
    {"eSimConnectionWarning", IDS_CELLULAR_SETUP_ESIM_CONNECTION_WARNING},
    {"scanQRCode", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE},
    {"scanQRCodeNoProfiles",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_NO_PROFILES},
    {"scanQRCodeEnterActivationCode",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_ENTER_ACTIVATION_CODE},
    {"switchCamera", IDS_CELLULAR_SETUP_ESIM_PAGE_SWITCH_CAMERA},
    {"useCamera", IDS_CELLULAR_SETUP_ESIM_PAGE_USE_CAMERA},
    {"scanQRCodeSuccess", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_SUCCESS},
    {"qrCodeUseCameraAgain",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_USE_CAMERA_AGAIN},
    {"scanQrCodeError", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_ERROR},
    {"qrCodeRetry", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_RETRY},
    {"scanQrCodeLoading", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_LOADING},
    {"scanQrCodeInvalid", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_INVALID},
    {"profileListPageMessage", IDS_CELLULAR_SETUP_PROFILE_LIST_PAGE_MESSAGE},
    {"eidPopupTitle", IDS_CELLULAR_SETUP_EID_POPUP_TITLE},
    {"eidPopupDescription", IDS_CELLULAR_SETUP_EID_POPUP_DESCRIPTION},
    {"closeEidPopupButtonLabel",
     IDS_CELLULAR_SETUP_CLOSE_EID_POPUP_BUTTON_LABEL},
    {"confirmationCodeMessage",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_MESSAGE},
    {"confirmationCodeInput",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_INPUT},
    {"confirmationCodeError",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_ERROR},
    {"confirmationCodeLoading",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_LOADING}};  // namespace

struct NamedBoolean {
  const char* name;
  bool value;
};

const std::vector<const NamedBoolean>& GetBooleanValues() {
  static const base::NoDestructor<std::vector<const NamedBoolean>> named_bools({
      {"updatedCellularActivationUi",
       chromeos::features::IsCellularActivationUiEnabled()},
  });
  return *named_bools;
}

}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStringsWithoutPlaceholders);
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : kLocalizedStringsWithoutPlaceholders)
    builder->Add(entry.name, entry.id);
}

void AddNonStringLoadTimeData(content::WebUIDataSource* html_source) {
  for (const auto& entry : GetBooleanValues())
    html_source->AddBoolean(entry.name, entry.value);
  html_source->AddBoolean(useExternalEuiccLoadTimeDataName,
                          base::FeatureList::IsEnabled(
                              chromeos::features::kCellularUseExternalEuicc));
}

void AddNonStringLoadTimeDataToDict(base::DictionaryValue* dict) {
  for (const auto& entry : GetBooleanValues())
    dict->SetBoolean(entry.name, entry.value);
}

}  // namespace cellular_setup
}  // namespace chromeos
