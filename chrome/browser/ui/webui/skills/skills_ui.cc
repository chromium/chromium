// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_ui.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"
#include "chrome/browser/ui/webui/skills/skills_page_handler.h"
#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/skills_resources.h"
#include "chrome/grit/skills_resources_map.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

namespace skills {
namespace {

constexpr int kMaxNameCharCount = 100;
constexpr int kMaxPromptCharCount = 20000;

std::string GetSkillsV2Origin() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSkillsV2Origin)) {
    return command_line->GetSwitchValueASCII(switches::kSkillsV2Origin);
  }
  // Default to prod if not specified.
  return "https://clients5.google.com";
}

bool ShouldDisableBrowseSkillsPage() {
  if (!base::FeatureList::IsEnabled(
          features::kSkills1PDisabledForNonEnLocales)) {
    return false;
  }

  // Disable the browse skills page if the current locale is not English.
  const ApplicationLocaleStorage& application_locale_storage =
      CHECK_DEREF(CHECK_DEREF(CHECK_DEREF(g_browser_process).GetFeatures())
                      .application_locale_storage());
  return !base::StartsWith(application_locale_storage.Get(), "en",
                           base::CompareCase::INSENSITIVE_ASCII);
}

void AddSkillsV1Resources(content::WebUIDataSource* source, Profile* profile) {
  source->AddResourcePath("dialog", IDR_SKILLS_SKILLS_DIALOG_HTML);
  source->AddBoolean("isGlicEnabled",
                     glic::GlicEnabling::IsReadyForProfile(profile));
  source->AddBoolean("isSkillsEnabled",
                     SkillsServiceFactory::IsSkillsEnabledForProfile(profile));
  source->AddInteger("MAX_NAME_CHAR_COUNT", kMaxNameCharCount);
  source->AddInteger("MAX_PROMPT_CHAR_COUNT", kMaxPromptCharCount);
  source->AddBoolean("isRefinementEnabled",
                     // Disable refinement whenever browseSkillsPage is disabled
                     // (non-en locales).
                     !ShouldDisableBrowseSkillsPage());
  source->AddBoolean(
      "isSubheadersEnabled",
      base::FeatureList::IsEnabled(features::kSkillsSubheadersEnabled));
  source->AddBoolean(
      "isAutocompleteEnabled",
      base::FeatureList::IsEnabled(features::kSkillsAutocomplete));
  source->AddBoolean("shouldDisableBrowseSkillsPage",
                     ShouldDisableBrowseSkillsPage());

  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"edit", IDS_EDIT2},
      {"menu", IDS_MENU},
      {"save", IDS_SAVE},
      {"delete", IDS_SKILL_PAGE_USER_SKILLS_DELETE},
      {"add", IDS_ADD},
      {"all", IDS_SKILLS_PAGE_ALL_CATEGORY},
      {"curatedByPrefix", IDS_SKILLS_CURATED_PREFIX},
      {"browseSkillsTitle", IDS_SKILL_PAGE_BROWSE_SKILLS_TITLE},
      {"firstPartyAddSkillErrorToast",
       IDS_SKILL_PAGE_FIRST_PARTY_ADD_SKILL_ERROR_TOAST},
      {"emptyStateTitle", IDS_SKILL_PAGE_EMPTY_STATE_TITLE},
      {"emptyStateDescription", IDS_SKILL_PAGE_EMPTY_STATE_DESCRIPTION},
      {"invalidSkillToastText", IDS_SKILL_PAGE_INVALID_SKILL_TOAST},
      {"userSkillsTitle", IDS_SKILL_PAGE_USER_SKILLS_TITLE},
      {"userSkillsDescription", IDS_SKILL_PAGE_USER_SKILLS_DESCRIPTION},
      {"searchBarPlaceholderText", IDS_SKILL_PAGE_SEARCH_BAR_PLACEHOLDER_TEXT},
      {"mainMenu", IDS_SKILL_PAGE_MAIN_MENU},
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

}  // namespace

SkillsUI::SkillsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/false,
                              /*enable_chrome_histograms=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISkillsHost);

  if (base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled)) {
    webui::SetupWebUIDataSource(source, kSkillsResources,
                                IDR_SKILLS_V2_SKILLS_HTML);
    source->AddResourcePath("dialog", IDR_SKILLS_V2_SKILLS_DIALOG_HTML);
    source->AddString("skillsPrimaryOrigin", GetSkillsV2Origin());
  } else {
    webui::SetupWebUIDataSource(source, kSkillsResources,
                                IDR_SKILLS_SKILLS_HTML);
    AddSkillsV1Resources(source, profile);
  }
  source->AddString("webuiRefresh2026", features::IsWebuiRefresh2026Enabled()
                                            ? "webui-refresh-2026"
                                            : "");
  source->AddBoolean(
      "isSkillsWebViewV2Enabled",
      base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled));

  std::string application_locale = g_browser_process->GetApplicationLocale();
  std::string google_locale = google_util::GetGoogleLocale(application_locale);
  source->AddString("languageCode", google_locale);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  source->AddBoolean("devMode", command_line->HasSwitch("skills-dev"));

  bool is_internal_user = false;
  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    is_internal_user = gaia::IsGoogleInternalAccountEmail(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
  }
  source->AddBoolean("isInternalUser", is_internal_user);

  // Shared strings for Skills V1/V2.
  // TODO(b/521780336): Remove search results strings once we migrate to v2.
  static constexpr webui::LocalizedString kStrings[] = {
      {"goToSettings", IDS_SKILLS_GO_TO_SETTINGS},
      {"skillsTitle", IDS_SKILL_PAGE_TITLE},
      {"errorPageTitle", IDS_SKILLS_ERROR_PAGE_TITLE},
      {"noSearchResultsTitle", IDS_SKILLS_NO_SEARCH_RESULT_TITLE},
      {"errorPageDescription", IDS_SKILLS_ERROR_PAGE_DESCRIPTION},
      {"noSearchResultsDescription", IDS_SKILLS_NO_SEARCH_RESULT_DESCRIPTION},
      {"disabledErrorPageDescription",
       IDS_SKILLS_DISABLED_ERROR_PAGE_DESCRIPTION},
      {"remoteAuthorityUnreachableDescription",
       IDS_SKILLS_REMOTE_AUTHORITY_UNREACHABLE_DESCRIPTION},
  };

  source->AddLocalizedStrings(kStrings);
}

void SkillsUI::InitializeDialog(base::WeakPtr<SkillsDialogDelegate> delegate,
                                Skill skill,
                                SkillsDialogEntryPoint entrypoint,
                                mojom::SkillsDialogType dialog_type) {
  delegate_ = delegate;
  initial_skill_ = std::move(skill);
  entrypoint_ = entrypoint;
  dialog_type_ = dialog_type;

  if (base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled)) {
    base::DictValue update =
        base::DictValue()
            .Set("dialogType", static_cast<int>(dialog_type))
            .Set("skillId", initial_skill_.id)
            .Set("skillPrompt", initial_skill_.prompt);
    content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                     chrome::kChromeUISkillsHost,
                                     std::move(update));
  }
}

void SkillsUI::BindInterface(
    mojo::PendingReceiver<skills::mojom::PageHandlerFactory> receiver) {
  CHECK(!base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled));
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SkillsUI::BindInterface(
    mojo::PendingReceiver<::skills::mojom::SkillsPageHandler> receiver) {
  CHECK(base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled));
  Profile* profile = Profile::FromWebUI(web_ui());
  page_handler_v2_ = std::make_unique<skills::SkillsPageHandlerV2>(
      std::move(receiver), profile,
      IdentityManagerFactory::GetForProfile(profile),
      web_ui()->GetWebContents(), delegate_);
}

void SkillsUI::CreatePageHandler(
    mojo::PendingRemote<skills::mojom::SkillsPage> page,
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver) {
  CHECK(!base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled));
  page_handler_ = std::make_unique<SkillsPageHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents());
}

void SkillsUI::CreateDialogHandler(
    mojo::PendingReceiver<skills::mojom::DialogHandler> receiver) {
  CHECK(!base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled));
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
