// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commander/commander_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/commander/commander_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/commander_resources.h"
#include "chrome/grit/commander_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

CommanderUI::CommanderUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  auto handler = std::make_unique<CommanderHandler>();
  handler_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUICommanderHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"placeholder", IDS_QUICK_COMMANDS_PLACEHOLDER},
      {"noResults", IDS_QUICK_COMMANDS_NO_RESULTS},
      {"pageTitle", IDS_QUICK_COMMANDS_LABEL},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);
  webui::SetupWebUIDataSource(
      source, base::make_span(kCommanderResources, kCommanderResourcesSize),
      IDR_COMMANDER_COMMANDER_HTML);
}

CommanderUI::~CommanderUI() = default;
