// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "printing/buildflags/buildflags.h"

namespace {

content::WebUIDataSource* CreateLocalDiscoveryHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDevicesHost);

  source->SetDefaultResource(IDR_LOCAL_DISCOVERY_HTML);
  source->AddResourcePath("local_discovery.css", IDR_LOCAL_DISCOVERY_CSS);
  source->AddResourcePath("local_discovery.js", IDR_LOCAL_DISCOVERY_JS);

  static constexpr LocalizedString kStrings[] = {
    {"serviceRegister", IDS_LOCAL_DISCOVERY_SERVICE_REGISTER},
    {"manageDevice", IDS_LOCAL_DISCOVERY_MANAGE_DEVICE},

    {"registerPrinterInformationMessage",
     IDS_CLOUD_PRINT_REGISTER_PRINTER_INFORMATION},
    {"registerUser", IDS_LOCAL_DISCOVERY_REGISTER_USER},
    {"confirmRegistration", IDS_LOCAL_DISCOVERY_CONFIRM_REGISTRATION},
    {"addingPrinter", IDS_LOCAL_DISCOVERY_ADDING_PRINTER},
    {"addingError", IDS_LOCAL_DISCOVERY_ERROR_OCURRED},
    {"addingErrorMessage", IDS_LOCAL_DISCOVERY_ERROR_OCURRED_MESSAGE},
    {"addingCanceledMessage", IDS_LOCAL_DISCOVERY_REGISTER_CANCELED_ON_PRINTER},
    {"addingTimeoutMessage", IDS_LOCAL_DISCOVERY_REGISTER_TIMEOUT_ON_PRINTER},
    {"addingPrinterMessage1", IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE1},
    {"addingPrinterMessage2", IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE2},
    {"devicesTitle", IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE},
    {"noDescriptionPrinter", IDS_LOCAL_DISCOVERY_NO_DESCRIPTION_PRINTER},
    {"printersOnNetworkZero", IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_ZERO},
    {"printersOnNetworkOne", IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_ONE},
    {"printersOnNetworkMultiple",
     IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_MULTIPLE},
    {"cancel", IDS_CANCEL},
    {"confirm", IDS_CONFIRM},
    {"ok", IDS_OK},
    {"loading", IDS_LOCAL_DISCOVERY_LOADING},
    {"addPrinters", IDS_LOCAL_DISCOVERY_ADD_PRINTERS},
    {"noPrintersOnNetworkExplanation",
     IDS_LOCAL_DISCOVERY_NO_PRINTERS_ON_NETWORK_EXPLANATION},
    {"cloudDevicesUnavailable", IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_UNAVAILABLE},
    {"retryLoadCloudDevices", IDS_LOCAL_DISCOVERY_RETRY_LOAD_CLOUD_DEVICES},
    {"cloudDevicesNeedLogin", IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_NEED_LOGIN},
    {"cloudDevicesLogin", IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_LOGIN},
    {"registerNeedLogin", IDS_LOCAL_DISCOVERY_REGISTER_NEED_LOGIN},
    {"availableDevicesTitle", IDS_LOCAL_DISCOVERY_AVAILABLE_DEVICES},
    {"myDevicesTitle", IDS_LOCAL_DISCOVERY_MY_DEVICES},

// Cloud print connector-related strings.
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
    {"cloudPrintConnectorEnablingButton",
     IDS_CLOUD_PRINT_CONNECTOR_ENABLING_BUTTON},
    {"cloudPrintConnectorDisabledButton",
     IDS_CLOUD_PRINT_CONNECTOR_DISABLED_BUTTON},
    {"cloudPrintConnectorEnabledButton",
     IDS_CLOUD_PRINT_CONNECTOR_ENABLED_BUTTON},
    {"cloudPrintName", IDS_GOOGLE_CLOUD_PRINT},
    {"titleConnector", IDS_LOCAL_DISCOVERY_CONNECTOR_SECTION},
#endif
  };
  AddLocalizedStringsBulk(source, kStrings, base::size(kStrings));

  source->UseStringsJs();

  source->DisableDenyXFrameOptions();

  return source;
}

}  // namespace

LocalDiscoveryUI::LocalDiscoveryUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://devices/ source.
  content::WebUIDataSource* source = CreateLocalDiscoveryHTMLSource();
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // TODO(gene): Use LocalDiscoveryUIHandler to send updated to the devices
  // page. For example
  web_ui->AddMessageHandler(
      std::make_unique<local_discovery::LocalDiscoveryUIHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

void LocalDiscoveryUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLocalDiscoveryNotificationsEnabled,
#if defined(OS_WIN)
      false,
#else
      true,
#endif
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
