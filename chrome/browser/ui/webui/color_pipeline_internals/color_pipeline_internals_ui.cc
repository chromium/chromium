// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/color_pipeline_internals/color_pipeline_internals_ui.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/color_pipeline_internals_resources.h"
#include "chrome/grit/color_pipeline_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ColorPipelineInternalsUIConfig::ColorPipelineInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUIColorPipelineInternalsHost) {}

ColorPipelineInternalsUI::ColorPipelineInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  auto* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIColorPipelineInternalsHost);

  webui::SetupWebUIDataSource(source, kColorPipelineInternalsResources,
                              IDR_COLOR_PIPELINE_INTERNALS_INDEX_HTML);
}

ColorPipelineInternalsUI::~ColorPipelineInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ColorPipelineInternalsUI)
