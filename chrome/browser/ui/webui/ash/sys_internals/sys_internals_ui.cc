// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_ui.h"

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_message_handler.h"
#include "chrome/grit/sys_internals_resources.h"
#include "chrome/grit/sys_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/resources/grit/webui_resources.h"
#include "ui/webui/webui_util.h"

namespace ash {

SysInternalsUI::SysInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SysInternalsMessageHandler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             ash::kChromeUISysInternalsHost);
  webui::EnableTrustedTypesCSP(html_source);

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  html_source->SetDefaultResource(IDR_SYS_INTERNALS_INDEX_HTML);
  html_source->AddResourcePaths(kSysInternalsResources);

  html_source->AddResourcePath("test_loader_util.js",
                               IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  base::RecordAction(base::UserMetricsAction("Open_Sys_Internals"));
}

SysInternalsUI::~SysInternalsUI() = default;

}  // namespace ash
