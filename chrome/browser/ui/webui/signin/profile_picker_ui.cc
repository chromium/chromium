// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "base/feature_list.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/profile_creation_customize_themes_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/profile_picker_resources.h"
#include "chrome/grit/profile_picker_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace {

// Miniumum size for the picker UI.
constexpr int kMinimumPickerSizePx = 620;

bool IsProfileCreationAllowed() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

bool IsGuestModeEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

void AddStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"mainViewTitle", IDS_PROFILE_PICKER_MAIN_VIEW_TITLE},
      {"mainViewSubtitle", IDS_PROFILE_PICKER_MAIN_VIEW_SUBTITLE},
      {"addSpaceButton", IDS_PROFILE_PICKER_ADD_SPACE_BUTTON},
      {"askOnStartupCheckboxText", IDS_PROFILE_PICKER_ASK_ON_STARTUP},
      {"browseAsGuestButton", IDS_PROFILE_PICKER_BROWSE_AS_GUEST_BUTTON},
      {"menu", IDS_MENU},
      {"profileMenuName", IDS_PROFILE_PICKER_PROFILE_MENU_BUTTON_NAME},
      {"profileMenuRemoveText", IDS_PROFILE_PICKER_PROFILE_MENU_REMOVE_TEXT},
      {"profileMenuCustomizeText",
       IDS_PROFILE_PICKER_PROFILE_MENU_CUSTOMIZE_TEXT},
      {"removeWarningLocalProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_LOCAL_PROFILE},
      {"removeWarningSignedInProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE},
      {"removeWarningHistory", IDS_PROFILE_PICKER_REMOVE_WARNING_HISTORY},
      {"removeWarningPasswords", IDS_PROFILE_PICKER_REMOVE_WARNING_PASSWORDS},
      {"removeWarningBookmarks", IDS_PROFILE_PICKER_REMOVE_WARNING_BOOKMARKS},
      {"removeWarningAutofill", IDS_PROFILE_PICKER_REMOVE_WARNING_AUTOFILL},
      {"removeWarningCalculating",
       IDS_PROFILE_PICKER_REMOVE_WARNING_CALCULATING},
      {"backButtonLabel", IDS_PROFILE_PICKER_BACK_BUTTON_LABEL},
      {"profileTypeChoiceTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_TITLE},
      {"profileTypeChoiceSubtitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_SUBTITLE},
      {"signInButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_SIGNIN_BUTTON_LABEL},
      {"notNowButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_NOT_NOW_BUTTON_LABEL},
      {"localProfileCreationTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_TITLE},
      {"localProfileCreationThemeText",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_THEME_TEXT},
      {"createProfileNamePlaceholder",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_INPUT_NAME},
      {"createDesktopShortcutLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_SHORTCUT_TEXT},
      {"createProfileConfirm",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_DONE},

      // Color picker.
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddBoolean("askOnStartup",
                          g_browser_process->local_state()->GetBoolean(
                              prefs::kBrowserShowProfilePickerOnStartup));
  html_source->AddBoolean(
      "signInProfileCreationFlowSupported",
      base::FeatureList::IsEnabled(features::kProfilesUIRevamp));

  html_source->AddString("minimumPickerSize",
                         base::StringPrintf("%ipx", kMinimumPickerSizePx));

  // Add policies.
  html_source->AddBoolean("isForceSigninEnabled",
                          signin_util::IsForceSigninEnabled());
  html_source->AddBoolean("isGuestModeEnabled", IsGuestModeEnabled());
  html_source->AddBoolean("isProfileCreationAllowed",
                          IsProfileCreationAllowed());
  html_source->AddBoolean("profileShortcutsEnabled",
                          ProfileShortcutManager::IsFeatureEnabled());
  // TODO(crbug.com/1063856): Check if |BrowserSignin| device policy exists.
}

}  // namespace

ProfilePickerUI::ProfilePickerUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      customize_themes_factory_receiver_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIProfilePickerHost);

  std::unique_ptr<ProfilePickerHandler> handler =
      std::make_unique<ProfilePickerHandler>();
  ProfilePickerHandler* raw_handler = handler.get();
  web_ui->AddMessageHandler(std::move(handler));

  if (web_ui->GetWebContents()->GetURL().query() ==
      chrome::kChromeUIProfilePickerStartupQuery) {
    raw_handler->EnableStartupMetrics();
  }

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/signin/profile_picker/";

  AddStrings(html_source);
#if BUILDFLAG(OPTIMIZE_WEBUI)
  webui::SetupBundledWebUIDataSource(
      html_source, "profile_picker.js",
      IDR_PROFILE_PICKER_PROFILE_PICKER_ROLLUP_JS,
      IDR_PROFILE_PICKER_PROFILE_PICKER_HTML);
  html_source->AddResourcePath("lazy_load.js",
                               IDR_PROFILE_PICKER_LAZY_LOAD_ROLLUP_JS);
  html_source->AddResourcePath("shared.rollup.js",
                               IDR_PROFILE_PICKER_SHARED_ROLLUP_JS);
  html_source->AddResourcePath("images/left_banner_image.svg",
                               IDR_PROFILE_PICKER_IMAGES_LEFT_BANNER_IMAGE);
  html_source->AddResourcePath("images/right_banner_image.svg",
                               IDR_PROFILE_PICKER_IMAGES_RIGHT_BANNER_IMAGE);
  html_source->AddResourcePath(
      "images/dark_mode_left_banner_image.svg",
      IDR_PROFILE_PICKER_IMAGES_DARK_MODE_LEFT_BANNER_IMAGE);
  html_source->AddResourcePath(
      "images/dark_mode_right_banner_image.svg",
      IDR_PROFILE_PICKER_IMAGES_DARK_MODE_RIGHT_BANNER_IMAGE);
  html_source->AddResourcePath(
      "profile_creation_flow/images/banner_light_image.svg",
      IDR_PROFILE_PICKER_PROFILE_CREATION_FLOW_IMAGES_BANNER_LIGHT_IMAGE);
  html_source->AddResourcePath(
      "profile_creation_flow/images/banner_dark_image.svg",
      IDR_PROFILE_PICKER_PROFILE_CREATION_FLOW_IMAGES_BANNER_DARK_IMAGE);
#else
  html_source->AddResourcePath("signin_icons.js", IDR_SIGNIN_ICONS_JS);
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kProfilePickerResources, kProfilePickerResourcesSize),
      generated_path, IDR_PROFILE_PICKER_PROFILE_PICKER_HTML);
#endif
  content::WebUIDataSource::Add(profile, html_source);
}

ProfilePickerUI::~ProfilePickerUI() = default;

// static
gfx::Size ProfilePickerUI::GetMinimumSize() {
  return gfx::Size(kMinimumPickerSizePx, kMinimumPickerSizePx);
}

void ProfilePickerUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound()) {
    customize_themes_factory_receiver_.reset();
  }
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}

void ProfilePickerUI::CreateCustomizeThemesHandler(
    mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
        pending_client,
    mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
        pending_handler) {
  customize_themes_handler_ =
      std::make_unique<ProfileCreationCustomizeThemesHandler>(
          std::move(pending_client), std::move(pending_handler));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProfilePickerUI)
