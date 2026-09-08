// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_ui.h"

#include <memory>
#include <string>

#include "ash/constants/webui_url_constants.h"
#include "base/check_deref.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_handler.h"
#include "chrome/grit/browser_resources.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kArcPowerControlJsPath[] = "arc_power_control.js";
constexpr char kArcPowerControlCssPath[] = "arc_power_control.css";
constexpr char kArcOverviewTracingUiJsPath[] = "arc_overview_tracing_ui.js";
constexpr char kArcTracingUiJsPath[] = "arc_tracing_ui.js";
constexpr char kArcTracingCssPath[] = "arc_tracing.css";

}  // anonymous namespace

namespace ash {

ArcPowerControlUIConfig::ArcPowerControlUIConfig(
    const ApplicationLocaleStorage* application_locale_storage)
    : WebUIConfig(content::kChromeUIScheme, ash::kChromeUIArcPowerControlHost),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

ArcPowerControlUIConfig::~ArcPowerControlUIConfig() = default;

bool ArcPowerControlUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return arc::IsArcAllowedForProfile(
      Profile::FromBrowserContext(browser_context));
}

std::unique_ptr<content::WebUIController>
ArcPowerControlUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                               const GURL& url) {
  return std::make_unique<ArcPowerControlUI>(
      web_ui, application_locale_storage_->Get());
}

ArcPowerControlUI::ArcPowerControlUI(content::WebUI* web_ui,
                                     const std::string& application_locale)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ArcPowerControlHandler>());

  content::WebUIDataSource* const source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             ash::kChromeUIArcPowerControlHost);
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

  base::DictValue localized_strings;
  webui::SetLoadTimeDataDefaults(application_locale, &localized_strings);
  source->AddLocalizedStrings(localized_strings);
}

ArcPowerControlUI::~ArcPowerControlUI() = default;

}  // namespace ash
