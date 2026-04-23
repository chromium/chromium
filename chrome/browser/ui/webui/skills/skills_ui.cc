// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_ui.h"

#include "base/i18n/number_formatting.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"
#include "chrome/browser/ui/webui/skills/skills_page_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/skills_resources.h"
#include "chrome/grit/skills_resources_map.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace skills {

constexpr int kMaxNameCharCount = 100;
constexpr int kMaxPromptCharCount = 20000;

SkillsUI::SkillsUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISkillsHost);
  webui::SetupWebUIDataSource(source, kSkillsResources, IDR_SKILLS_SKILLS_HTML);
  source->AddResourcePath("dialog", IDR_SKILLS_SKILLS_DIALOG_HTML);
  source->AddBoolean("isGlicEnabled",
                     glic::GlicEnabling::IsReadyForProfile(profile));
  source->AddInteger("MAX_NAME_CHAR_COUNT", kMaxNameCharCount);
  source->AddInteger("MAX_PROMPT_CHAR_COUNT", kMaxPromptCharCount);
  source->AddBoolean(
      "isRefinementEnabled",
      base::FeatureList::IsEnabled(features::kSkillsRefinementEnabled));

  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  source->AddBoolean(
      "isAutocompleteEnabled",
      base::FeatureList::IsEnabled(features::kSkillsAutocomplete));
  source->AddBoolean(
      "isPartnerPicksEnabled",
      base::FeatureList::IsEnabled(features::kSkillsPartnerPicks));
  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"edit", IDS_EDIT2},
      {"menu", IDS_MENU},
      {"save", IDS_SAVE},
      {"delete", IDS_SKILL_PAGE_USER_SKILLS_DELETE},
      {"add", IDS_ADD},
      {"browseSkillsTitle", IDS_SKILL_PAGE_BROWSE_SKILLS_TITLE},
      // TODO(b/503394871): Remove this string.
      {"topPicksTitle", IDS_SKILL_PAGE_BROWSE_SKILLS_TOP_PICKS_TITLE},
      {"firstPartyAddSkillErrorToast",
       IDS_SKILL_PAGE_FIRST_PARTY_ADD_SKILL_ERROR_TOAST},
      {"emptyStateTitle", IDS_SKILL_PAGE_EMPTY_STATE_TITLE},
      {"emptyStateDescription", IDS_SKILL_PAGE_EMPTY_STATE_DESCRIPTION},
      {"invalidSkillToastText", IDS_SKILL_PAGE_INVALID_SKILL_TOAST},
      {"userSkillsTitle", IDS_SKILL_PAGE_USER_SKILLS_TITLE},
      {"userSkillsDescription", IDS_SKILL_PAGE_USER_SKILLS_DESCRIPTION},
      {"searchBarPlaceholderText", IDS_SKILL_PAGE_SEARCH_BAR_PLACEHOLDER_TEXT},
      {"skillsTitle", IDS_SKILL_PAGE_TITLE},
      {"mainMenu", IDS_SKILL_PAGE_MAIN_MENU},
      {"errorPageTitle", IDS_SKILLS_ERROR_PAGE_TITLE},
      {"errorPageDescription", IDS_SKILLS_ERROR_PAGE_DESCRIPTION},
      {"footerText", IDS_SKILLS_SIDEBAR_FOOTER_TEXT},
      {"footerBranding", IDS_SKILLS_SIDEBAR_FOOTER_BRANDING},
      {"addSkillHeader", IDS_SKILLS_DIALOG_ADD_SKILL_HEADER},
      {"editSkillHeader", IDS_SKILLS_DIALOG_EDIT_SKILL_HEADER},
      {"skillDescription", IDS_SKILLS_DIALOG_DESCRIPTION},
      {"name", IDS_SKILLS_DIALOG_NAME_LABEL},
      {"namePlaceholder", IDS_SKILLS_DIALOG_NAME_PLACEHOLDER},
      {"chooseIcon", IDS_SKILLS_DIALOG_CHOOSE_ICON_TOOLTIP},
      {"instructions", IDS_SKILLS_DIALOG_INSTRUCTIONS_LABEL},
      {"instructionsPlaceholder", IDS_SKILLS_DIALOG_INSTRUCTIONS_PLACEHOLDER},
      {"undo", IDS_SKILLS_DIALOG_UNDO_TOOLTIP},
      {"redo", IDS_SKILLS_DIALOG_REDO_TOOLTIP},
      {"refine", IDS_SKILLS_DIALOG_REFINE_TOOLTIP},
      {"refineError", IDS_SKILLS_DIALOG_REFINE_ERROR},
      {"accountInfo", IDS_SKILLS_DIALOG_ACCOUNT_INFO},
      {"copyInstructions", IDS_SKILL_PAGE_USER_SKILLS_COPY_INSTRUCTIONS},
      {"skillCardActionMenuLabel", IDS_SKILL_CARD_ACTION_MENU_LABEL},
      {"skillAddNewSkillLabel", IDS_ADD_NEW_SKILL_LABEL},
      {"noSearchResultsTitle", IDS_SKILLS_NO_SEARCH_RESULT_TITLE},
      {"noSearchResultsDescription", IDS_SKILLS_NO_SEARCH_RESULT_DESCRIPTION},
      {"saveError", IDS_SKILLS_DIALOG_SAVE_ERROR},
      {"emojiSearchPlaceholder", IDS_SKILLS_EMOJI_PICKER_SEARCH_PLACEHOLDER},
      {"emojiPickerAriaLabel", IDS_SKILLS_EMOJI_PICKER_ARIA_LABEL},
  };

  source->AddLocalizedStrings(kStrings);
  source->AddString(
      "nameCharLimitError",
      l10n_util::GetStringFUTF16(IDS_SKILLS_DIALOG_CHAR_LIMIT_ERROR,
                                 base::FormatNumber(kMaxNameCharCount)));
  source->AddString(
      "charLimitError",
      l10n_util::GetStringFUTF16(IDS_SKILLS_DIALOG_CHAR_LIMIT_ERROR,
                                 base::FormatNumber(kMaxPromptCharCount)));
}

void SkillsUI::InitializeDialog(base::WeakPtr<SkillsDialogDelegate> delegate,
                                Skill skill,
                                SkillsDialogEntryPoint entrypoint,
                                mojom::SkillsDialogType dialog_type) {
  delegate_ = delegate;
  initial_skill_ = std::move(skill);
  entrypoint_ = entrypoint;
  dialog_type_ = dialog_type;
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
      initial_skill_, entrypoint_, dialog_type_, delegate_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SkillsUI)

SkillsUI::~SkillsUI() = default;

bool SkillsUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kSkillsEnabled);
}

}  // namespace skills
