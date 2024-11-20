// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internal_debug_pages_disabled/internal_debug_pages_disabled_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/internal_debug_pages_disabled_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

void CreateAndAddHTMLSource(Profile* profile, const std::string& host_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, host_name);
  source->AddLocalizedString("pageHeading",
                             IDS_INTERNAL_DEBUG_PAGES_DISABLED_HEADING);

  source->AddResourcePath("", IDR_INTERNAL_DEBUG_PAGES_DISABLED_APP_HTML);
}

}  // namespace

InternalDebugPagesDisabledUI::InternalDebugPagesDisabledUI(
    content::WebUI* web_ui,
    const std::string& host_name)
    : WebUIController(web_ui) {
  CreateAndAddHTMLSource(Profile::FromWebUI(web_ui), host_name);
}
