// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/projector/projector_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/projector/grit/projector_resources.h"
#include "chromeos/projector/grit/projector_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIProjectorSelfieCamHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kProjectorResources, kProjectorResourcesSize),
      IDR_PROJECTOR_SELFIE_CAM_HTML);
  return source;
}

}  // namespace

ProjectorUI::ProjectorUI(content::WebUI* web_ui)
    : MojoBubbleWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateProjectorHTMLSource());
}

ProjectorUI::~ProjectorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ProjectorUI)

}  // namespace chromeos
