// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dlp_internals/dlp_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui_data_source.h"

namespace policy {

DlpInternalsUI::DlpInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::CreateAndAdd(profile,
                                         chrome::kChromeUIDlpInternalsHost);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DlpInternalsUI)

DlpInternalsUI::~DlpInternalsUI() = default;

}  // namespace policy
