// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/device_log/device_log_ui.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/device_log_resources.h"
#include "chrome/grit/device_log_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {

DeviceLogUIConfig::DeviceLogUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIDeviceLogHost) {}

namespace {

class DeviceLogMessageHandler : public content::WebUIMessageHandler {
 public:
  DeviceLogMessageHandler() {}

  DeviceLogMessageHandler(const DeviceLogMessageHandler&) = delete;
  DeviceLogMessageHandler& operator=(const DeviceLogMessageHandler&) = delete;

  ~DeviceLogMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getLog", base::BindRepeating(&DeviceLogMessageHandler::GetLog,
                                      base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "clearLog", base::BindRepeating(&DeviceLogMessageHandler::ClearLog,
                                        base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    web_ui()->RegisterMessageCallback(
        "isLacrosEnabled",
        base::BindRepeating(&DeviceLogMessageHandler::IsLacrosEnabled,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "openBrowserDeviceLog",
        base::BindRepeating(&DeviceLogMessageHandler::OpenBrowserDevieLog,
                            base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

 private:
  void GetLog(const base::Value::List& value) {
    AllowJavascript();
    std::string callback_id = value[0].GetString();
    base::Value data(device_event_log::GetAsString(
        device_event_log::NEWEST_FIRST, "json", "",
        device_event_log::LOG_LEVEL_DEBUG, 0));
    ResolveJavascriptCallback(base::Value(callback_id), data);
  }

  void ClearLog(const base::Value::List& value) const {
    device_event_log::ClearAll();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void IsLacrosEnabled(const base::Value::List& value) {
    AllowJavascript();
    const bool is_lacros_enabled = crosapi::browser_util::IsLacrosEnabled();
    std::string callback_id = value[0].GetString();
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(is_lacros_enabled));
  }

  void OpenBrowserDevieLog(const base::Value::List& args) const {
    // Note: This will only be called by the UI when Lacros is available.
    DCHECK(crosapi::BrowserManager::Get());
    crosapi::BrowserManager::Get()->SwitchToTab(
        GURL(chrome::kChromeUIDeviceLogUrl),
        /*path_behavior=*/NavigateParams::RESPECT);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace

DeviceLogUI::DeviceLogUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DeviceLogMessageHandler>());

  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDeviceLogHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"titleText", IDS_DEVICE_LOG_TITLE},
      {"autoRefreshText", IDS_DEVICE_AUTO_REFRESH},
      {"autoSelectTypes", IDS_DEVICE_SELECT_TYPES},
      {"logRefreshText", IDS_DEVICE_LOG_REFRESH},
      {"logClearText", IDS_DEVICE_LOG_CLEAR},
      {"logClearTypesText", IDS_DEVICE_LOG_CLEAR_TYPES},
      {"logNoEntriesText", IDS_DEVICE_LOG_NO_ENTRIES},
      {"logLevelLabel", IDS_DEVICE_LOG_LEVEL_LABEL},
      {"logLevelErrorText", IDS_DEVICE_LOG_LEVEL_ERROR},
      {"logLevelUserText", IDS_DEVICE_LOG_LEVEL_USER},
      {"logLevelEventText", IDS_DEVICE_LOG_LEVEL_EVENT},
      {"logLevelDebugText", IDS_DEVICE_LOG_LEVEL_DEBUG},
      {"logLevelFileinfoText", IDS_DEVICE_LOG_FILEINFO},
      {"logLevelTimeDetailText", IDS_DEVICE_LOG_TIME_DETAIL},
      {"logTypeLoginText", IDS_DEVICE_LOG_TYPE_LOGIN},
      {"logTypeNetworkText", IDS_DEVICE_LOG_TYPE_NETWORK},
      {"logTypePowerText", IDS_DEVICE_LOG_TYPE_POWER},
      {"logTypeBluetoothText", IDS_DEVICE_LOG_TYPE_BLUETOOTH},
      {"logTypeUsbText", IDS_DEVICE_LOG_TYPE_USB},
      {"logTypeHidText", IDS_DEVICE_LOG_TYPE_HID},
      {"logTypePrinterText", IDS_DEVICE_LOG_TYPE_PRINTER},
      {"logTypeFidoText", IDS_DEVICE_LOG_TYPE_FIDO},
      {"logTypeSerialText", IDS_DEVICE_LOG_TYPE_SERIAL},
      {"logTypeCameraText", IDS_DEVICE_LOG_TYPE_CAMERA},
      {"logTypeGeolocationText", IDS_DEVICE_LOG_TYPE_GEOLOCATION},
      {"logTypeExtensionsText", IDS_DEVICE_LOG_TYPE_EXTENSIONS},
      {"logTypeDisplayText", IDS_DEVICE_LOG_TYPE_DISPLAY},
      {"logTypeFirmwareText", IDS_DEVICE_LOG_TYPE_FIRMWARE},
      {"logEntryFormat", IDS_DEVICE_LOG_ENTRY},
  };
  html->AddLocalizedStrings(kStrings);

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string device_log_url(chrome::kChromeUIDeviceLogUrl16);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  std::u16string device_log_url(chrome::kOsUIDeviceLogURL);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  auto os_link_container = l10n_util::GetStringFUTF16(
      IDS_DEVICE_LOG_OS_LINK_CONTAINER, device_log_url);
  html->AddString("osLinkContainer", os_link_container);
#endif  // BUILDFLAG(IS_CHROMEOS)

  html->UseStringsJs();
  html->AddResourcePaths(base::make_span(kDeviceLogResources,
                                         kDeviceLogResourcesSize));
  html->SetDefaultResource(IDR_DEVICE_LOG_DEVICE_LOG_UI_HTML);
}

DeviceLogUI::~DeviceLogUI() {
}

}  // namespace chromeos
