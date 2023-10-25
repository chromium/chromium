// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_ui.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

SysInternalsUI::SysInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SysInternalsMessageHandler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUISysInternalsHost);
  webui::EnableTrustedTypesCSP(html_source);

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  html_source->AddResourcePath("", IDR_SYS_INTERNALS_HTML);
  html_source->AddResourcePath("index.html", IDR_SYS_INTERNALS_HTML);
  html_source->AddResourcePath("index.css", IDR_SYS_INTERNALS_CSS);
  html_source->AddResourcePath("index.js", IDR_SYS_INTERNALS_JS);
  html_source->AddResourcePath("main.js", IDR_SYS_INTERNALS_MAIN_JS);
  html_source->AddResourcePath("constants.js", IDR_SYS_INTERNALS_CONSTANT_JS);
  html_source->AddResourcePath("types.js", IDR_SYS_INTERNALS_TYPES_JS);
  html_source->AddResourcePath("line_chart.css",
                               IDR_SYS_INTERNALS_LINE_CHART_CSS);

  html_source->AddResourcePath("line_chart/constants.js",
                               IDR_SYS_INTERNALS_LINE_CHART_CONSTANTS_JS);
  html_source->AddResourcePath("line_chart/data_series.js",
                               IDR_SYS_INTERNALS_LINE_CHART_DATA_SERIES_JS);
  html_source->AddResourcePath("line_chart/unit_label.js",
                               IDR_SYS_INTERNALS_LINE_CHART_UNIT_LABEL_JS);
  html_source->AddResourcePath("line_chart/line_chart.js",
                               IDR_SYS_INTERNALS_LINE_CHART_LINE_CHART_JS);
  html_source->AddResourcePath("line_chart/menu.js",
                               IDR_SYS_INTERNALS_LINE_CHART_MENU_JS);
  html_source->AddResourcePath("line_chart/scrollbar.js",
                               IDR_SYS_INTERNALS_LINE_CHART_SCROLLBAR_JS);
  html_source->AddResourcePath("line_chart/sub_chart.js",
                               IDR_SYS_INTERNALS_LINE_CHART_SUB_CHART_JS);

  html_source->AddResourcePath("images/menu.svg",
                               IDR_SYS_INTERNALS_IMAGE_MENU_SVG);
  html_source->AddResourcePath("images/info.svg",
                               IDR_SYS_INTERNALS_IMAGE_INFO_SVG);
  html_source->AddResourcePath("images/cpu.svg",
                               IDR_SYS_INTERNALS_IMAGE_CPU_SVG);
  html_source->AddResourcePath("images/memory.svg",
                               IDR_SYS_INTERNALS_IMAGE_MEMORY_SVG);
  html_source->AddResourcePath("images/zram.svg",
                               IDR_SYS_INTERNALS_IMAGE_ZRAM_SVG);

  html_source->AddResourcePath("test_loader_util.js",
                               IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  base::RecordAction(base::UserMetricsAction("Open_Sys_Internals"));
}

SysInternalsUI::~SysInternalsUI() {}

}  // namespace ash
