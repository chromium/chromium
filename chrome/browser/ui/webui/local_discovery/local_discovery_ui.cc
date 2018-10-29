// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
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

  source->AddLocalizedString("serviceRegister",
                             IDS_LOCAL_DISCOVERY_SERVICE_REGISTER);
  source->AddLocalizedString("manageDevice", IDS_LOCAL_DISCOVERY_MANAGE_DEVICE);

  source->AddLocalizedString("registerPrinterInformationMessage",
                             IDS_CLOUD_PRINT_REGISTER_PRINTER_INFORMATION);
  source->AddLocalizedString("registerUser",
                             IDS_LOCAL_DISCOVERY_REGISTER_USER);
  source->AddLocalizedString("confirmRegistration",
                             IDS_LOCAL_DISCOVERY_CONFIRM_REGISTRATION);
  source->AddLocalizedString("addingPrinter",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER);
  source->AddLocalizedString("addingError",
                             IDS_LOCAL_DISCOVERY_ERROR_OCURRED);
  source->AddLocalizedString("addingErrorMessage",
                             IDS_LOCAL_DISCOVERY_ERROR_OCURRED_MESSAGE);
  source->AddLocalizedString("addingCanceledMessage",
                             IDS_LOCAL_DISCOVERY_REGISTER_CANCELED_ON_PRINTER);
  source->AddLocalizedString("addingTimeoutMessage",
                             IDS_LOCAL_DISCOVERY_REGISTER_TIMEOUT_ON_PRINTER);
  source->AddLocalizedString("addingPrinterMessage1",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE1);
  source->AddLocalizedString("addingPrinterMessage2",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE2);
  source->AddLocalizedString("devicesTitle",
                             IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE);
  source->AddLocalizedString("noDescriptionPrinter",
                             IDS_LOCAL_DISCOVERY_NO_DESCRIPTION_PRINTER);
  source->AddLocalizedString("printersOnNetworkZero",
                             IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_ZERO);
  source->AddLocalizedString("printersOnNetworkOne",
                             IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_ONE);
  source->AddLocalizedString("printersOnNetworkMultiple",
                             IDS_LOCAL_DISCOVERY_PRINTERS_ON_NETWORK_MULTIPLE);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("confirm", IDS_CONFIRM);
  source->AddLocalizedString("ok", IDS_OK);
  source->AddLocalizedString("loading", IDS_LOCAL_DISCOVERY_LOADING);
  source->AddLocalizedString("addPrinters", IDS_LOCAL_DISCOVERY_ADD_PRINTERS);
  source->AddLocalizedString(
      "noPrintersOnNetworkExplanation",
      IDS_LOCAL_DISCOVERY_NO_PRINTERS_ON_NETWORK_EXPLANATION);
  source->AddLocalizedString("cloudDevicesUnavailable",
                             IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_UNAVAILABLE);
  source->AddLocalizedString("retryLoadCloudDevices",
                             IDS_LOCAL_DISCOVERY_RETRY_LOAD_CLOUD_DEVICES);
  source->AddLocalizedString("cloudDevicesNeedLogin",
                             IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_NEED_LOGIN);
  source->AddLocalizedString("cloudDevicesLogin",
                             IDS_LOCAL_DISCOVERY_CLOUD_DEVICES_LOGIN);
  source->AddLocalizedString("registerNeedLogin",
                             IDS_LOCAL_DISCOVERY_REGISTER_NEED_LOGIN);
  source->AddLocalizedString("availableDevicesTitle",
                             IDS_LOCAL_DISCOVERY_AVAILABLE_DEVICES);
  source->AddLocalizedString("myDevicesTitle",
                             IDS_LOCAL_DISCOVERY_MY_DEVICES);

  // Cloud print connector-related strings.
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
  source->AddLocalizedString("cloudPrintConnectorEnablingButton",
                             IDS_CLOUD_PRINT_CONNECTOR_ENABLING_BUTTON);
  source->AddLocalizedString("cloudPrintConnectorDisabledButton",
                             IDS_CLOUD_PRINT_CONNECTOR_DISABLED_BUTTON);
  source->AddLocalizedString("cloudPrintConnectorEnabledButton",
                             IDS_CLOUD_PRINT_CONNECTOR_ENABLED_BUTTON);
  source->AddLocalizedString("cloudPrintName",
                             IDS_GOOGLE_CLOUD_PRINT);
  source->AddLocalizedString("titleConnector",
                             IDS_LOCAL_DISCOVERY_CONNECTOR_SECTION);
#endif

  source->SetJsonPath("strings.js");

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
