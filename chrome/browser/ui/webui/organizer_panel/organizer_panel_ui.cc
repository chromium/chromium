// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_ui.h"

#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/organizer_panel_resources.h"
#include "chrome/grit/organizer_panel_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/webui/webui_util.h"

OrganizerPanelUIConfig::OrganizerPanelUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIOrganizerPanelHost) {}

OrganizerPanelUI::OrganizerPanelUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIOrganizerPanelHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
  };
  source->AddLocalizedStrings(kStrings);

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());

  webui::SetupWebUIDataSource(source, kOrganizerPanelResources,
                              IDR_ORGANIZER_PANEL_ORGANIZER_PANEL_HTML);
}

OrganizerPanelUI::~OrganizerPanelUI() = default;
