// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/legion_internals/legion_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/legion_internals_resources.h"
#include "chrome/grit/legion_internals_resources_map.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

LegionInternalsUIConfig::LegionInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUILegionInternalsHost) {}

LegionInternalsUIConfig::~LegionInternalsUIConfig() = default;

LegionInternalsUI::LegionInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the chrome://legion-internals source.
  content::WebUIDataSource* internals = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUILegionInternalsHost);

  webui::SetupWebUIDataSource(internals, base::span(kLegionInternalsResources),
                              IDR_LEGION_INTERNALS_LEGION_INTERNALS_HTML);
}

LegionInternalsUI::~LegionInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LegionInternalsUI)
