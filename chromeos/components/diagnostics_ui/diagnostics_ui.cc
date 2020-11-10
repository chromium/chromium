// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/diagnostics_ui.h"

#include <string>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/diagnostics_ui/backend/diagnostics_manager.h"
#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "chromeos/components/diagnostics_ui/url_constants.h"
#include "chromeos/grit/chromeos_diagnostics_app_resources.h"
#include "chromeos/grit/chromeos_diagnostics_app_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

namespace {

constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chromeos/components/diagnostics_ui/resources/";

void AddDiagnosticsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"adapterStatus", IDS_DIAGNOSTICS_POWER_LABEL},
      {"batteryTitle", IDS_DIAGNOSTICS_BATTERY_TITLE},
      {"chargeFullDesign", IDS_DIAGNOSTICS_DESIGNED_FULL_CHARGE_LABEL},
      {"chargeFullNow", IDS_DIAGNOSTICS_NOW_FULL_CHARGE_LABEL},
      {"chargeNow", IDS_DIAGNOSTICS_CHARGE_NOW_LABEL},
      {"cpuTemp", IDS_DIAGNOSTICS_CPU_TEMPERATURE_LABEL},
      {"cpuTitle", IDS_DIAGNOSTICS_CPU_TITLE},
      {"cpuUsage", IDS_DIAGNOSTICS_CPU_USAGE_LABEL},
      {"cpuUsageSystem", IDS_DIAGNOSTICS_CPU_USAGE_SYSTEM_LABEL},
      {"cpuUsageUser", IDS_DIAGNOSTICS_CPU_USAGE_USER_LABEL},
      {"currentNow", IDS_DIAGNOSTICS_CURRENT_NOW_LABEL},
      {"cycleCount", IDS_DIAGNOSTICS_CYCLE_COUNT_LABEL},
      {"diagnosticsTitle", IDS_DIAGNOSTICS_TITLE},
      {"memoryAvailable", IDS_DIAGNOSTICS_MEMORY_AVAILABLE_LABEL},
      {"memoryTitle", IDS_DIAGNOSTICS_MEMORY_TITLE},
      {"percentageLabel", IDS_DIAGNOSTICS_PERCENTAGE_LABEL},
      {"powerTime", IDS_DIAGNOSTICS_POWER_TIME_LABEL},
      {"remainingCharge", IDS_DIAGNOSTICS_REMAINING_CHARGE_LABEL},
      {"sessionLog", IDS_DIAGNOSTICS_SESSION_LOG_LABEL},
      {"totalMemory", IDS_DIAGNOSTICS_TOTAL_MEMORY_LABEL},
      {"usedMemory", IDS_DIAGNOSTICS_USED_MEMORY_LABEL},
  };
  for (const auto& str : kLocalizedStrings) {
    html_source->AddLocalizedString(str.name, str.id);
  }
  html_source->UseStringsJs();
}
// TODO(jimmyxgong): Replace with webui::SetUpWebUIDataSource() once it no
// longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource) {
  for (const auto& resource : resources) {
    std::string path = resource.name;
    if (path.rfind(generated_path, 0) == 0)
      path = path.substr(generated_path.size());

    source->AddResourcePath(path, resource.value);
  }

  source->SetDefaultResource(default_resource);
  source->AddResourcePath("d3.min.js", IDR_D3_SRC_D3_MIN_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
}

}  // namespace

DiagnosticsUI::DiagnosticsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui),
      diagnostics_manager_(
          std::make_unique<diagnostics::DiagnosticsManager>()) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIDiagnosticsAppHost));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources = base::make_span(kChromeosDiagnosticsAppResources,
                                         kChromeosDiagnosticsAppResourcesSize);
  SetUpWebUIDataSource(html_source.get(), resources, kGeneratedPath,
                       IDR_DIAGNOSTICS_APP_INDEX_HTML);

  html_source->AddResourcePath(
      "system_data_provider.mojom-lite.js",
      IDR_DIAGNOSTICS_SYSTEM_DATA_PROVIDER_MOJO_LITE_JS);
  html_source->AddResourcePath(
      "system_routine_controller.mojom-lite.js",
      IDR_DIAGNOSTICS_SYSTEM_ROUTINE_CONTROLLER_MOJO_LITE_JS);

  AddDiagnosticsStrings(html_source.get());
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

DiagnosticsUI::~DiagnosticsUI() = default;

void DiagnosticsUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::SystemDataProvider> receiver) {
  diagnostics::SystemDataProvider* system_data_provider =
      diagnostics_manager_->GetSystemDataProvider();
  if (system_data_provider) {
    system_data_provider->BindInterface(std::move(receiver));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiagnosticsUI)

}  // namespace chromeos
