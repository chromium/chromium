// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

ManageMirrorSyncUI::ManageMirrorSyncUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManageMirrorSyncHost);
  auto* profile = Profile::FromWebUI(web_ui);
  source->SetDefaultResource(IDR_MANAGE_MIRRORSYNC_INDEX_HTML);

  content::WebUIDataSource::Add(profile, source);
}

ManageMirrorSyncUI::~ManageMirrorSyncUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ManageMirrorSyncUI)

}  // namespace chromeos
