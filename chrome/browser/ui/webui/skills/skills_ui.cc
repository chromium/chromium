// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/webui/skills/skills_page_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/skills_resources.h"
#include "chrome/grit/skills_resources_map.h"
#include "components/skills/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace skills {

void AddDialogStringResources(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"save", IDS_SAVE},
  };

  source->AddLocalizedStrings(kStrings);
}

SkillsUI::SkillsUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUISkillsHost);
  webui::SetupWebUIDataSource(source, kSkillsResources, IDR_SKILLS_SKILLS_HTML);
  source->AddResourcePath("dialog", IDR_SKILLS_SKILLS_DIALOG_HTML);
  AddDialogStringResources(source);
}

void SkillsUI::BindInterface(
    mojo::PendingReceiver<skills::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SkillsUI::CreatePageHandler(
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SkillsPageHandler>(
      std::move(receiver), skills::SkillsServiceFactory::GetForProfile(
                               Profile::FromWebUI(web_ui())));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SkillsUI)

SkillsUI::~SkillsUI() = default;

bool SkillsUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kSkillsEnabled);
}

}  // namespace skills
