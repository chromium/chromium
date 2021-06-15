// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/whats_new_resources.h"
#include "chrome/grit/whats_new_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

content::WebUIDataSource* CreateWhatsNewUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWhatsNewHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kWhatsNewResources, kWhatsNewResourcesSize),
      IDR_WHATS_NEW_WHATS_NEW_HTML);
  return source;
}

}  // namespace

WhatsNewUI::WhatsNewUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      CreateWhatsNewUIHtmlSource(Profile::FromWebUI(web_ui));
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

WhatsNewUI::~WhatsNewUI() = default;
