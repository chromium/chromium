// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_ui.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kArcPowerControlJsPath[] = "arc_power_control.js";
constexpr char kArcPowerControlCssPath[] = "arc_power_control.css";
constexpr char kArcOverviewTracingUiJsPath[] = "arc_overview_tracing_ui.js";
constexpr char kArcTracingUiJsPath[] = "arc_tracing_ui.js";
constexpr char kArcTracingCssPath[] = "arc_tracing.css";

void CreateAndAddPowerControlDataSource(Profile* profile) {
  content::WebUIDataSource* const source =
      content::WebUIDataSource::CreateAndAdd(
          profile, chrome::kChromeUIArcPowerControlHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_ARC_POWER_CONTROL_HTML);
  source->AddResourcePath(kArcPowerControlJsPath, IDR_ARC_POWER_CONTROL_JS);
  source->AddResourcePath(kArcPowerControlCssPath, IDR_ARC_POWER_CONTROL_CSS);
  source->AddResourcePath(kArcOverviewTracingUiJsPath,
                          IDR_ARC_OVERVIEW_TRACING_UI_JS);
  source->AddResourcePath(kArcTracingCssPath, IDR_ARC_TRACING_CSS);
  source->AddResourcePath(kArcTracingUiJsPath, IDR_ARC_TRACING_UI_JS);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  base::Value::Dict localized_strings;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  source->AddLocalizedStrings(localized_strings);
}

}  // anonymous namespace

namespace ash {

bool ArcPowerControlUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return arc::IsArcAllowedForProfile(
      Profile::FromBrowserContext(browser_context));
}

ArcPowerControlUI::ArcPowerControlUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ArcPowerControlHandler>());
  CreateAndAddPowerControlDataSource(Profile::FromWebUI(web_ui));
}

}  // namespace ash
