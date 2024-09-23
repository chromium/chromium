// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/enterprise_reporting_resources.h"
#include "chrome/grit/enterprise_reporting_resources_map.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace {
// Returns the device information to be displayed on the
// chrome://enterprise-reporting page.
base::Value::Dict GetDeviceInfo(content::WebUI* web_ui) {
  base::Value::Dict device_info;
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();

  device_info.Set("revision", version_info::GetLastChange());
  device_info.Set("version", version_info::GetVersionNumber());
  device_info.Set(
      "clientId",
      reporting::GetUserClientId(Profile::FromWebUI(web_ui)).value_or(""));
  device_info.Set("directoryId", connector->GetDirectoryApiID());
  device_info.Set("enrollmentDomain",
                  connector->GetEnterpriseEnrollmentDomain());
  device_info.Set("obfuscatedCustomerId", connector->GetObfuscatedCustomerID());

  return device_info;
}
}  // namespace

namespace ash::reporting {

EnterpriseReportingUI::EnterpriseReportingUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  DCHECK(base::FeatureList::IsEnabled(ash::features::kEnterpriseReportingUI));
  // Set up the chrome://enterprise-reporting source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIEnterpriseReportingHost);

  // Populate device info.
  std::string device_info_json;
  base::JSONWriter::Write(GetDeviceInfo(web_ui), &device_info_json);
  html_source->AddString("deviceInfo", device_info_json);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kEnterpriseReportingResources,
                      kEnterpriseReportingResourcesSize),
      IDR_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_HTML);
}

EnterpriseReportingUI::~EnterpriseReportingUI() = default;

bool EnterpriseReportingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ash::features::kEnterpriseReportingUI);
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseReportingUI)

void EnterpriseReportingUI::BindInterface(
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandlerFactory>
        receiver) {
  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void EnterpriseReportingUI::CreatePageHandler(
    mojo::PendingRemote<enterprise_reporting::mojom::Page> page,
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver) {
  DCHECK(page);
  // Ensure sequencing for `EnterpriseReportingPageHandler` in order to make it
  // capable of using weak pointers.
  page_handler_ = EnterpriseReportingPageHandler::Create(std::move(receiver),
                                                         std::move(page));
}
}  // namespace ash::reporting
