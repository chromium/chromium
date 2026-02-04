// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_ui.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"
#include "chrome/browser/ui/webui/skills/skills_page_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/skills_resources.h"
#include "chrome/grit/skills_resources_map.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

namespace skills {

SkillsUI::SkillsUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISkillsHost);
  webui::SetupWebUIDataSource(source, kSkillsResources, IDR_SKILLS_SKILLS_HTML);
  source->AddResourcePath("dialog", IDR_SKILLS_SKILLS_DIALOG_HTML);
  bool isGlicEnabled = false;
#if BUILDFLAG(ENABLE_GLIC)
  isGlicEnabled = glic::GlicEnabling::IsEnabledForProfile(profile);
#endif
  source->AddBoolean("isGlicEnabled", isGlicEnabled);
  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"edit", IDS_EDIT2},
      {"save", IDS_SAVE},
      {"delete", IDS_DELETE},
      {"add", IDS_ADD},
      {"browseSkillsTitle", IDS_SKILL_PAGE_BROWSE_SKILLS_TITLE},
      {"topPicksTitle", IDS_SKILL_PAGE_BROWSE_SKILLS_TOP_PICKS_TITLE},
      {"firstPartyAddSkillErrorToast",
       IDS_SKILL_PAGE_FIRST_PARTY_ADD_SKILL_ERROR_TOAST},
      {"emptyStateTitle", IDS_SKILL_PAGE_EMPTY_STATE_TITLE},
      {"emptyStateDescription", IDS_SKILL_PAGE_EMPTY_STATE_DESCRIPTION},
      {"userSkillsTitle", IDS_SKILL_PAGE_USER_SKILLS_TITLE},
      {"userSkillsDescription", IDS_SKILL_PAGE_USER_SKILLS_DESCRIPTION},
      {"searchBarPlaceholderText", IDS_SKILL_PAGE_SEARCH_BAR_PLACEHOLDER_TEXT},
      {"skillsTitle", IDS_SKILL_PAGE_TITLE},
      {"mainMenu", IDS_SKILL_PAGE_MAIN_MENU},
      {"errorPageTitle", IDS_SKILLS_ERROR_PAGE_TITLE},
      {"errorPageDescription", IDS_SKILLS_ERROR_PAGE_DESCRIPTION},
  };

  source->AddLocalizedStrings(kStrings);
}

void SkillsUI::InitializeDialog(base::WeakPtr<SkillsDialogDelegate> delegate,
                                Skill skill) {
  delegate_ = delegate;
  initial_skill_ = std::move(skill);
}

void SkillsUI::BindInterface(
    mojo::PendingReceiver<skills::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SkillsUI::CreatePageHandler(
    mojo::PendingRemote<skills::mojom::SkillsPage> page,
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SkillsPageHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents());
}

void SkillsUI::CreateDialogHandler(
    mojo::PendingReceiver<skills::mojom::DialogHandler> receiver) {
  dialog_handler_ = std::make_unique<SkillsDialogHandler>(
      std::move(receiver), web_ui()->GetWebContents(),
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui())),
      std::move(initial_skill_), delegate_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SkillsUI)

SkillsUI::~SkillsUI() = default;

bool SkillsUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kSkillsEnabled);
}

}  // namespace skills
