// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/profile_picker_resources.h"
#include "chrome/grit/profile_picker_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

// Miniumum size for the picker UI.
constexpr int kMinimumPickerSizePx = 620;

bool IsBrowserSigninAllowed() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  policy::PolicyService* policy_service = g_browser_process->policy_service();
  DCHECK(policy_service);
  const policy::PolicyMap& policies = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const base::Value* browser_signin_value = policies.GetValue(
      policy::key::kBrowserSignin, base::Value::Type::INTEGER);

  if (!browser_signin_value)
    return true;

  return static_cast<policy::BrowserSigninMode>(
             browser_signin_value->GetInt()) !=
         policy::BrowserSigninMode::kDisabled;
#endif
}

std::string GetManagedDeviceDisclaimer() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> manager = chrome::GetSessionManagerIdentity();
  int managed_id =
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_SESSION_MANAGED_DESCRIPTION;
  int managed_by_id =
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_SESSION_MANAGED_BY_DESCRIPTION;
#else
  std::optional<std::string> manager = chrome::GetDeviceManagerIdentity();
  int managed_id =
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_DEVICE_MANAGED_DESCRIPTION;
  int managed_by_id =
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_DEVICE_MANAGED_BY_DESCRIPTION;
#endif
  if (!manager)
    return std::string();
  if (manager->empty()) {
    return l10n_util::GetStringUTF8(managed_id);
  }
  return l10n_util::GetStringFUTF8(managed_by_id, base::UTF8ToUTF16(*manager));
}

int GetMainViewTitleId() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return IDS_PROFILE_PICKER_MAIN_VIEW_TITLE_LACROS;
#else
  return ProfilePicker::Shown() ? IDS_PROFILE_PICKER_MAIN_VIEW_TITLE_V2
                                : IDS_PROFILE_PICKER_MAIN_VIEW_TITLE;
#endif
}

void AddStrings(content::WebUIDataSource* html_source) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int profile_type_choice_subtitle =
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_SUBTITLE_LACROS;
#else
  int profile_type_choice_subtitle =
      base::FeatureList::IsEnabled(switches::kExplicitBrowserSigninUIOnDesktop)
          ? IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_SUBTITLE_UNO
          : IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_SUBTITLE;
#endif

  static webui::LocalizedString kLocalizedStrings[] = {
      {"mainViewSubtitle",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
       IDS_PROFILE_PICKER_MAIN_VIEW_SUBTITLE_LACROS
#else
       IDS_PROFILE_PICKER_MAIN_VIEW_SUBTITLE
#endif
      },
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
      {"profileMenuAriaLabel",
       IDS_PROFILE_PICKER_PROFILE_MORE_ACTIONS_ARIA_LABEL},
      {"profileMenuRemoveText", IDS_PROFILE_PICKER_PROFILE_MENU_REMOVE_TEXT},
      {"profileMenuCustomizeText",
       IDS_PROFILE_PICKER_PROFILE_MENU_CUSTOMIZE_TEXT},
      {"removeWarningLocalProfileTitle",
       IDS_PROFILE_PICKER_REMOVE_WARNING_LOCAL_PROFILE_TITLE},
      {"removeWarningSignedInProfileTitle",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE_TITLE_LACROS
#else
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE_TITLE
#endif
      },
      {"removeWarningHistory", IDS_PROFILE_PICKER_REMOVE_WARNING_HISTORY},
      {"removeWarningPasswords", IDS_PROFILE_PICKER_REMOVE_WARNING_PASSWORDS},
      {"removeWarningBookmarks", IDS_PROFILE_PICKER_REMOVE_WARNING_BOOKMARKS},
      {"removeWarningAutofill", IDS_PROFILE_PICKER_REMOVE_WARNING_AUTOFILL},
      {"removeWarningCalculating",
       IDS_PROFILE_PICKER_REMOVE_WARNING_CALCULATING},
      {"backButtonAriaLabel", IDS_PROFILE_PICKER_BACK_BUTTON_ARIA_LABEL},
      {"profileTypeChoiceTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_PROFILE_TYPE_CHOICE_TITLE},
      {"profileTypeChoiceSubtitle", profile_type_choice_subtitle},
      {"notNowButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_NOT_NOW_BUTTON_LABEL},
      {"profileSwitchTitle", IDS_PROFILE_PICKER_PROFILE_SWITCH_TITLE},
      {"profileSwitchSubtitle", IDS_PROFILE_PICKER_PROFILE_SWITCH_SUBTITLE},
      {"switchButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_SWITCH_SWITCH_BUTTON_LABEL},

#if BUILDFLAG(IS_CHROMEOS_LACROS)
      {"accountSelectionLacrosTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_ACCOUNT_SELECTION_LACROS_TITLE},
      {"accountSelectionLacrosOtherAccountButtonLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_ACCOUNT_SELECTION_LACROS_OTHER_ACCOUNT_BUTTON_LABEL},
      {"lacrosPrimaryProfileDeletionWarningTitle",
       IDS_PROFILE_PICKER_LACROS_PRIMARY_PROFILE_DELETION_WARNING_TITLE},
      {"lacrosPrimaryProfileDeletionWarning",
       IDS_PROFILE_PICKER_LACROS_PRIMARY_PROFILE_DELETION_WARNING},
      {"lacrosPrimaryProfileDeletionWarningConfirmation",
       IDS_PROFILE_PICKER_LACROS_PRIMARY_PROFILE_DELETION_WARNING_CONFIRMATION},
#else
      {"removeWarningLocalProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_LOCAL_PROFILE},
      {"removeWarningSignedInProfile",
       IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE},
#endif
      {"ok", IDS_OK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddLocalizedString("mainViewTitle", GetMainViewTitleId());

  html_source->AddLocalizedString(
      "signInButtonLabel",
      IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_SIGNIN_BUTTON_LABEL);

  ProfilePicker::AvailabilityOnStartup availability_on_startup =
      static_cast<ProfilePicker::AvailabilityOnStartup>(
          g_browser_process->local_state()->GetInteger(
              prefs::kBrowserProfilePickerAvailabilityOnStartup));
  bool ask_on_startup_allowed =
      availability_on_startup == ProfilePicker::AvailabilityOnStartup::kEnabled;
  html_source->AddBoolean("askOnStartup",
                          g_browser_process->local_state()->GetBoolean(
                              prefs::kBrowserShowProfilePickerOnStartup));
  html_source->AddBoolean("profilesReorderingEnabled",
                          base::FeatureList::IsEnabled(kProfilesReordering));
  html_source->AddBoolean("signInProfileCreationFlowSupported",
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                          AccountConsistencyModeManager::IsDiceSignInAllowed());
#else
                          true);
#endif

  html_source->AddString("minimumPickerSize",
                         base::StringPrintf("%ipx", kMinimumPickerSizePx));

  html_source->AddInteger("placeholderAvatarIndex",
                          profiles::GetPlaceholderAvatarIndex());

  html_source->AddString("managedDeviceDisclaimer",
                         GetManagedDeviceDisclaimer());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::string remove_warning_profile = l10n_util::GetStringFUTF8(
      IDS_PROFILE_PICKER_REMOVE_WARNING_SIGNED_IN_PROFILE_LACROS,
      ui::GetChromeOSDeviceName(),
      l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE),
      l10n_util::GetStringUTF16(IDS_OS_SETTINGS_PEOPLE_V2));
  html_source->AddString("removeWarningProfileLacros", remove_warning_profile);
  html_source->AddString("deviceType", ui::GetChromeOSDeviceName());

  bool guest_mode_enabled = true;
  // Device settings may be nullptr in tests.
  if (crosapi::mojom::DeviceSettings* device_settings =
          g_browser_process->browser_policy_connector()->GetDeviceSettings()) {
    if (device_settings->device_guest_mode_enabled ==
        crosapi::mojom::DeviceSettings::OptionalBool::kFalse) {
      guest_mode_enabled = false;
    }
  }
  const int account_selection_lacros_subtitle =
      guest_mode_enabled
          ? IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_ACCOUNT_SELECTION_LACROS_SUBTITLE_WITH_GUEST
          : IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_ACCOUNT_SELECTION_LACROS_SUBTITLE;
  html_source->AddLocalizedString("accountSelectionLacrosSubtitle",
                                  account_selection_lacros_subtitle);

#endif

  // Add policies.
  html_source->AddBoolean("isBrowserSigninAllowed", IsBrowserSigninAllowed());
  html_source->AddBoolean("isForceSigninEnabled",
                          signin_util::IsForceSigninEnabled());
  html_source->AddBoolean("isGuestModeEnabled", profiles::IsGuestModeEnabled());
  html_source->AddBoolean("isProfileCreationAllowed",
                          profiles::IsProfileCreationAllowed());
  html_source->AddBoolean("profileShortcutsEnabled",
                          ProfileShortcutManager::IsFeatureEnabled());
  html_source->AddBoolean("isAskOnStartupAllowed", ask_on_startup_allowed);

  html_source->AddResourcePath("images/left_banner.svg",
                               IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  html_source->AddResourcePath("images/left_banner_dark.svg",
                               IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  html_source->AddResourcePath("images/right_banner.svg",
                               IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  html_source->AddResourcePath("images/right_banner_dark.svg",
                               IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
}

}  // namespace

ProfilePickerUI::ProfilePickerUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          profile, chrome::kChromeUIProfilePickerHost);

  std::unique_ptr<ProfilePickerHandler> handler =
      std::make_unique<ProfilePickerHandler>();
  profile_picker_handler_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));

  // GetVisibleURL is used here because a WebUIController is created before the
  // navigation commits.
  if (web_ui->GetWebContents()->GetVisibleURL().query() ==
      chrome::kChromeUIProfilePickerStartupQuery) {
    profile_picker_handler_->EnableStartupMetrics();
  }

  // Setting the title here instead of relying on the one provided from the
  // page itself makes it available much earlier, and avoids having to fallback
  // to the one obtained from `NavigationEntry::GetTitleForDisplay()` (which
  // ends up being the URL) when we try to get it on startup for a11y purposes.
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(GetMainViewTitleId()));

  AddStrings(html_source);
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kProfilePickerResources, kProfilePickerResourcesSize),
      IDR_PROFILE_PICKER_PROFILE_PICKER_HTML);
}

ProfilePickerUI::~ProfilePickerUI() = default;

void ProfilePickerUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{});
}

void ProfilePickerUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

// static
gfx::Size ProfilePickerUI::GetMinimumSize() {
  return gfx::Size(kMinimumPickerSizePx, kMinimumPickerSizePx);
}

ProfilePickerHandler* ProfilePickerUI::GetProfilePickerHandlerForTesting() {
  return profile_picker_handler_;
}

void ProfilePickerUI::ShowForceSigninErrorDialog(
    const ForceSigninUIError& error) {
  profile_picker_handler_->DisplayForceSigninErrorDialog(base::FilePath(),
                                                         error);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProfilePickerUI)
