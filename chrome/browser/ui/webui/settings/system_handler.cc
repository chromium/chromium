// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/system_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace settings {

SystemHandler::SystemHandler() {}

SystemHandler::~SystemHandler() {}

// static
void SystemHandler::AddLoadTimeData(content::WebUIDataSource* data_source) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  data_source->AddBoolean("hardwareAccelerationEnabledAtStartup",
      g_browser_process->gpu_mode_manager()->initial_gpu_mode_pref());
#endif
}

void SystemHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showProxySettings",
      base::BindRepeating(&SystemHandler::HandleShowProxySettings,
                          base::Unretained(this)));
}

void SystemHandler::HandleShowProxySettings(const base::Value::List& args) {
  base::RecordAction(base::UserMetricsAction("Options_ShowProxySettings"));
  settings_utils::ShowNetworkProxySettings(web_ui()->GetWebContents());
}

}  // namespace settings
