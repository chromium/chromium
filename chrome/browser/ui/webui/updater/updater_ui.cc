// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_ui.h"

#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/updater_resources.h"
#include "chrome/grit/updater_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

UpdaterUI::UpdaterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUpdaterHost);

  webui::SetupWebUIDataSource(source, kUpdaterResources,
                              IDR_UPDATER_UPDATER_HTML);
}

UpdaterUI::~UpdaterUI() = default;
