// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {
content::WebUIDataSource* CreateSupportToolHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISupportToolHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  return source;
}
}  // namespace

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://support-tool/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateSupportToolHTMLSource());
}

SupportToolUI::~SupportToolUI() = default;
