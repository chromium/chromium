// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_ui.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing.h"
#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kArcGraphicsTracingJsPath[] = "arc_graphics_tracing.js";
constexpr char kArcGraphicsTracingUiJsPath[] = "arc_graphics_tracing_ui.js";
constexpr char kArcOverviewTracingJsPath[] = "arc_overview_tracing.js";
constexpr char kArcOverviewTracingUiJsPath[] = "arc_overview_tracing_ui.js";
constexpr char kArcTracingUiJsPath[] = "arc_tracing_ui.js";
constexpr char kArcTracingCssPath[] = "arc_tracing.css";

content::WebUIDataSource* CreateGraphicsDataSource() {
  content::WebUIDataSource* const source =
      content::WebUIDataSource::Create(chrome::kChromeUIArcGraphicsTracingHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_ARC_GRAPHICS_TRACING_HTML);
  source->AddResourcePath(kArcGraphicsTracingJsPath,
                          IDR_ARC_GRAPHICS_TRACING_JS);
  source->AddResourcePath(kArcGraphicsTracingUiJsPath,
                          IDR_ARC_GRAPHICS_TRACING_UI_JS);
  source->AddResourcePath(kArcTracingCssPath, IDR_ARC_TRACING_CSS);
  source->AddResourcePath(kArcTracingUiJsPath, IDR_ARC_TRACING_UI_JS);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  base::Value::Dict localized_strings;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  source->AddLocalizedStrings(localized_strings);

  return source;
}

content::WebUIDataSource* CreateOverviewDataSource() {
  content::WebUIDataSource* const source =
      content::WebUIDataSource::Create(chrome::kChromeUIArcOverviewTracingHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_ARC_OVERVIEW_TRACING_HTML);
  source->AddResourcePath(kArcOverviewTracingJsPath,
                          IDR_ARC_OVERVIEW_TRACING_JS);
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

  return source;
}

}  // anonymous namespace

namespace ash {

template <>
ArcGraphicsTracingUIConfig<
    ArcGraphicsTracingMode::kFull>::ArcGraphicsTracingUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIArcGraphicsTracingHost) {}

template <>
ArcGraphicsTracingUIConfig<
    ArcGraphicsTracingMode::kOverview>::ArcGraphicsTracingUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIArcOverviewTracingHost) {}

template <ArcGraphicsTracingMode mode>
bool ArcGraphicsTracingUIConfig<mode>::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return arc::IsArcAllowedForProfile(
      Profile::FromBrowserContext(browser_context));
}

template <>
ArcGraphicsTracingUI<ArcGraphicsTracingMode::kFull>::ArcGraphicsTracingUI(
    content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ArcGraphicsTracingHandler>(
      ArcGraphicsTracingMode::kFull));
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateGraphicsDataSource());
}

template <>
ArcGraphicsTracingUI<ArcGraphicsTracingMode::kOverview>::ArcGraphicsTracingUI(
    content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ArcGraphicsTracingHandler>(
      ArcGraphicsTracingMode::kOverview));
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateOverviewDataSource());
}

}  // namespace ash
