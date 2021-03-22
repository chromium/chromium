// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "base/feature_list.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profile_picker.h"
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
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace {

// Miniumum size for the picker UI.
constexpr int kMinimumPickerSizePx = 620;

bool IsBrowserSigninAllowed() {
  policy::PolicyService* policy_service = g_browser_process->policy_service();
  DCHECK(policy_service);
  const policy::PolicyMap& policies = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const base::Value* browser_signin_value =
      policies.GetValue(policy::key::kBrowserSignin);

  if (!browser_signin_value)
    return true;

  int int_browser_signin_value;
  bool success = browser_signin_value->GetAsInteger(&int_browser_signin_value);
  DCHECK(success);

  return static_cast<policy::BrowserSigninMode>(int_browser_signin_value) !=
         policy::BrowserSigninMode::kDisabled;
}

bool IsSignInProfileCreationFlowSupported() {
  return AccountConsistencyModeManager::IsDiceSignInAllowed() &&
         base::FeatureList::IsEnabled(features::kSignInProfileCreation);
}

std::string GetManagedDeviceDisclaimer() {
  if (!base::FeatureList::IsEnabled(features::kSignInProfileCreationEnterprise))
    return std::string();
  base::Optional<std::string> device_manager =
      chrome::GetDeviceManagerIdentity();
  if (!device_manager)
    return std::string();
  if (device_manager->empty()) {
    return l10n_util::GetStringUTF8(
        IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_DEVICE_MANAGED_DESCRIPTION);
  }
  return l10n_util::GetStringFUTF8(
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_DEVICE_MANAGED_BY_DESCRIPTION,
      base::UTF8ToUTF16(*device_manager));
}

void AddStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"mainViewSubtitle", IDS_PROFILE_PICKER_MAIN_VIEW_SUBTITLE},
      {"addSpaceButton", IDS_PROFILE_PICKER_ADD_SPACE_BUTTON},
      {"askOnStartupCheckboxText", IDS_PROFILE_PICKER_ASK_ON_STARTUP},
      {"browseAsGuestButton", IDS_PROFILE_PICKER_BROWSE_AS_GUEST_BUTTON},
      {"needsSigninPrompt",
       IDS_PROFILE_PICKER_PROFILE_CARD_NEEDS_SIGNIN_PROMPT},
      {"profileCardButtonLabel", IDS_PROFILE_PICKER_PROFILE_CARD_LABEL},
      {"profileCardInputLabel", IDS_PROFILE_PICKER_PROFILE_CARD_INPUT_LABEL},
      {"menu", IDS_MENU},
      {"cancel", IDS_CANCEL},
      {"profileMenuName", IDS_SETTINGS_MORE_ACTIONS},
      {"profileMenuRemoveText", IDS_PROFILE_PICKER_PROFILE_MENU_REMOVE_TEXT},
      {"profileMenuCustomizeText",
       IDS_PROFILE_PICKER_PROFILE_MENU_CUSTOMIZE_TEXT},
      {"removeWarningLocalProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_LOCAL_PROFILE},
      {"removeWarningLocalProfileTitle",
       IDS_PROFILE_PICKER_REMOVE_WARNING_LOCAL_PROFILE_TITLE},
      {"removeWarningSignedInProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE},
      {"removeWarningSignedInProfileTitle",
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE_TITLE},
      {"removeWarningHistory", IDS_PROFILE_PICKER_REMOVE_WARNING_HISTORY},
      {"removeWarningPasswords", IDS_PROFILE_PICKER_REMOVE_WARNING_PASSWORDS},
      {"removeWarningBookmarks", IDS_PROFILE_PICKER_REMOVE_WARNING_BOOKMARKS},
      {"removeWarningAutofill", IDS_PROFILE_PICKER_REMOVE_WARNING_AUTOFILL},
      {"removeWarningCalculating",
       IDS_PROFILE_PICKER_REMOVE_WARNING_CALCULATING},
      {"backButtonAriaLabel", IDS_PROFILE_PICKER_BACK_BUTTON_ARIA_LABEL},
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
      {"localProfileCreationCustomizeAvatarLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_CUSTOMIZE_AVATAR_BUTTON_LABEL},
      {"localProfileCreationThemeText",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_THEME_TEXT},
      {"createProfileNamePlaceholder",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_INPUT_NAME},
      {"createDesktopShortcutLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_SHORTCUT_TEXT},
      {"createProfileConfirm",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_DONE},
      {"defaultAvatarLabel", IDS_DEFAULT_AVATAR_LABEL_26},
      {"selectAnAvatarDialogTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_AVATAR_TEXT},
      {"selectAvatarDoneButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_AVATAR_DONE},
      {"profileSwitchTitle", IDS_PROFILE_PICKER_PROFILE_SWITCH_TITLE},
      {"profileSwitchSubtitle", IDS_PROFILE_PICKER_PROFILE_SWITCH_SUBTITLE},
      {"switchButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_SWITCH_SWITCH_BUTTON_LABEL},

      // Color picker.
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddLocalizedString("mainViewTitle",
                                  ProfilePicker::Shown()
                                      ? IDS_PROFILE_PICKER_MAIN_VIEW_TITLE_V2
                                      : IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);

  ProfilePicker::AvailabilityOnStartup availability_on_startup =
      static_cast<ProfilePicker::AvailabilityOnStartup>(
          g_browser_process->local_state()->GetInteger(
              prefs::kBrowserProfilePickerAvailabilityOnStartup));
  bool disable_ask_on_startup =
      availability_on_startup !=
          ProfilePicker::AvailabilityOnStartup::kEnabled ||
      !base::FeatureList::IsEnabled(kEnableProfilePickerOnStartupFeature);
  html_source->AddBoolean("disableAskOnStartup", disable_ask_on_startup);
  html_source->AddBoolean("askOnStartup",
                          g_browser_process->local_state()->GetBoolean(
                              prefs::kBrowserShowProfilePickerOnStartup));
  html_source->AddBoolean("signInProfileCreationFlowSupported",
                          IsSignInProfileCreationFlowSupported());

  html_source->AddString("minimumPickerSize",
                         base::StringPrintf("%ipx", kMinimumPickerSizePx));

  html_source->AddInteger("placeholderAvatarIndex",
                          profiles::GetPlaceholderAvatarIndex());

  html_source->AddString("managedDeviceDisclaimer",
                         GetManagedDeviceDisclaimer());

  // Add policies.
  html_source->AddBoolean("isBrowserSigninAllowed", IsBrowserSigninAllowed());
  html_source->AddBoolean("isForceSigninEnabled",
                          signin_util::IsForceSigninEnabled());
  html_source->AddBoolean("isGuestModeEnabled", profiles::IsGuestModeEnabled());
  html_source->AddBoolean("isProfileCreationAllowed",
                          profiles::IsProfileCreationAllowed());
  html_source->AddBoolean("profileShortcutsEnabled",
                          ProfileShortcutManager::IsFeatureEnabled());
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
  profile_picker_handler_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));

  if (web_ui->GetWebContents()->GetURL().query() ==
      chrome::kChromeUIProfilePickerStartupQuery) {
    profile_picker_handler_->EnableStartupMetrics();
  }

  AddStrings(html_source);
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kProfilePickerResources, kProfilePickerResourcesSize),
      IDR_PROFILE_PICKER_PROFILE_PICKER_HTML);
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

ProfilePickerHandler* ProfilePickerUI::GetProfilePickerHandlerForTesting() {
  return profile_picker_handler_;
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
