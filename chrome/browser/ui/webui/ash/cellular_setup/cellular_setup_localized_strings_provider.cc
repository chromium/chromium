// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"

#include <vector>

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

namespace ash::cellular_setup {
namespace {

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
    {"eSimFinalPageSuccessHeader",
     IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_SUCCESS_HEADER},
    {"eSimFinalPageMessage", IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_MESSAGE},
    {"eSimFinalPageErrorMessage",
     IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_ERROR_MESSAGE},
    {"scanQRCode", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE},
    {"scanQRCodeNoProfilesFound",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_NO_PROFILES_FOUND},
    {"enterActivationCode", IDS_CELLULAR_SETUP_ESIM_PAGE_ENTER_ACTIVATION_CODE},
    {"enterActivationCodeNoProfilesFound",
     IDS_CELLULAR_SETUP_ESIM_PAGE_ENTER_ACTIVATION_CODE_NO_PROFILES_FOUND},
    {"switchCamera", IDS_CELLULAR_SETUP_ESIM_PAGE_SWITCH_CAMERA},
    {"qrCodeA11YCameraOn", IDS_CELLULAR_SETUP_ESIM_PAGE_A11Y_QR_CODE_CAMERA_ON},
    {"qrCodeA11YCameraScanSuccess",
     IDS_CELLULAR_SETUP_ESIM_PAGE_A11Y_QR_CODE_CAMERA_SCAN_SUCCESS},
    {"useCamera", IDS_CELLULAR_SETUP_ESIM_PAGE_USE_CAMERA},
    {"scanQRCodeSuccess", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_SUCCESS},
    {"qrCodeUseCameraAgain",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_USE_CAMERA_AGAIN},
    {"scanQrCodeError", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_ERROR},
    {"qrCodeRetry", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_RETRY},
    {"scanQrCodeInvalid", IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_INVALID},
    {"scanQrCodeInputSubtitle",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_INPUT_SUBTITLE},
    {"scanQrCodeInputError",
     IDS_CELLULAR_SETUP_ESIM_PAGE_SCAN_QR_CODE_INPUT_ERROR},
    {"profileListPageMessage", IDS_CELLULAR_SETUP_PROFILE_LIST_PAGE_MESSAGE},
    {"profileListPageMessageWithLink",
     IDS_CELLULAR_SETUP_PROFILE_LIST_PAGE_MESSAGE_WITH_LINK},
    {"confirmationCodeMessage",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_MESSAGE},
    {"confirmationCodeInput",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_INPUT},
    {"confirmationCodeErrorLegacy",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_ERROR_LEGACY},
    {"confirmationCodeError",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_ERROR},
    {"confirmationCodeLoading",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CONFIRMATION_CODE_LOADING},
    {"profileInstallingMessage",
     IDS_CELLULAR_SETUP_ESIM_PROFILE_INSTALLING_MESSAGE},
    {"profileDiscoveryConsentTitle",
     IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_TITLE},
    {"profileDiscoveryConsentMessageWithLink",
     IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_MESSAGE_WITH_LINK},
    {"profileDiscoveryConsentScan",
     IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_SCAN},
    {"profileDiscoveryConsentCancel",
     IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_CANCEL},
    {"profileDiscoveryPageTitle",
     IDS_CELLULAR_SETUP_PROFILE_DISCOVERY_PAGE_TITLE},
    {"confimationCodePageTitle",
     IDS_CELLULAR_SETUP_CONFIRMATION_CODE_PAGE_TITLE},
    {"profileLoadingPageTitle", IDS_CELLULAR_SETUP_ESIM_PROFILE_LOADING_TITLE},
    {"profileLoadingPageMessage",
     IDS_CELLULAR_SETUP_ESIM_PROFILE_LOADING_MESSAGE},
    {"eSimCarrierLockedDevice",
     IDS_CELLULAR_SETUP_ESIM_PAGE_CARRIER_LOCKED_DEVICE}};

struct NamedBoolean {
  const char* name;
  bool value;
};

struct NamedResourceId {
  const char* name;
  int value;
};

const std::vector<NamedBoolean>& GetBooleanValues() {
  static const base::NoDestructor<std::vector<NamedBoolean>> named_bools(
      {{"useSecondEuicc",
        base::FeatureList::IsEnabled(features::kCellularUseSecondEuicc)}});
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
}

void AddNonStringLoadTimeDataToDict(base::Value::Dict* dict) {
  for (const auto& entry : GetBooleanValues())
    dict->SetByDottedPath(entry.name, entry.value);
}

}  // namespace ash::cellular_setup
