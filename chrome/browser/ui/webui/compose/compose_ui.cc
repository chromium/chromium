// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/compose/compose_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/compose_resources.h"
#include "chrome/grit/compose_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

ComposeUI::ComposeUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIComposeHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kComposeResources, kComposeResourcesSize),
      IDR_COMPOSE_COMPOSE_HTML);
  webui::SetupChromeRefresh2023(source);
}

ComposeUI::~ComposeUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ComposeUI)
