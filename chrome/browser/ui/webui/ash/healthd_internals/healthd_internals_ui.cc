// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/healthd_internals/healthd_internals_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/healthd_internals_resources.h"
#include "chrome/grit/healthd_internals_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

HealthdInternalsUI::HealthdInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIHealthdInternalsHost);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kHealthdInternalsResources,
                                              kHealthdInternalsResourcesSize),
                              IDR_HEALTHD_INTERNALS_HEALTHD_INTERNALS_HTML);
}

HealthdInternalsUI::~HealthdInternalsUI() = default;

}  // namespace ash
