// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/extended_updates_resources.h"
#include "chrome/grit/extended_updates_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash::extended_updates {

ExtendedUpdatesUI::ExtendedUpdatesUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIExtendedUpdatesDialogHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kExtendedUpdatesResources, kExtendedUpdatesResourcesSize),
      IDR_EXTENDED_UPDATES_EXTENDED_UPDATES_HTML);
}

ExtendedUpdatesUI::~ExtendedUpdatesUI() = default;

ExtendedUpdatesUIConfig::ExtendedUpdatesUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIExtendedUpdatesDialogHost) {}

ExtendedUpdatesUIConfig::~ExtendedUpdatesUIConfig() = default;

bool ExtendedUpdatesUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  // TODO(b/322418004): Also gate on user pref.
  return ash::features::IsExtendedUpdatesRequireOptInEnabled();
}

}  // namespace ash::extended_updates
