// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commander/commander.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/browser/uma_helper.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "ui/display/screen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kBookmarksMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kDownloadsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kHistoryMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kMoreToolsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kIncognitoMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel,
                                      kPasswordAndAutofillMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kPasswordManagerMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kShowSearchCompanion);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kPerformanceMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kPerformanceMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kChromeLabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kReadingModeMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kManageExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kVisitChromeWebStoreMenuItem);

namespace {

// Conditionally return the update app menu item title based on upgrade detector
// state.
std::u16string GetUpgradeDialogMenuItemName() {
  if (UpgradeDetector::GetInstance()->is_outdated_install() ||
      UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_MENU_ITEM);
  } else {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    return l10n_util::GetStringUTF16(
        base::FeatureList::IsEnabled(features::kUpdateTextOptions)
            ? IDS_RELAUNCH_TO_UPDATE_ALT
            : IDS_RELAUNCH_TO_UPDATE);
#else
    return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE);
#endif
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the menu item name for Lacros data migration.
std::u16string GetLacrosDataMigrationMenuItemName() {
  return l10n_util::GetStringUTF16(IDS_LACROS_DATA_MIGRATION_RELAUNCH);
}
#endif

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available.
std::u16string GetInstallPWALabel(const Browser* browser) {
  // There may be no active web contents in tests.
  auto* const web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::u16string();
  }

  const std::u16string app_name =
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents);
  return app_name.empty() ? app_name
                          : l10n_util::GetStringFUTF16(
                                IDS_INSTALL_TO_OS_LAUNCH_SURFACE,
                                ui::EscapeMenuLabelAmpersands(app_name));
}

// Returns the appropriate menu label for the IDC_OPEN_IN_PWA_WINDOW command if
// available.
std::u16string GetOpenPWALabel(const Browser* browser) {
  absl::optional<webapps::AppId> app_id =
      web_app::GetWebAppForActiveTab(browser);
  if (!app_id.has_value()) {
    return std::u16string();
  }

  // Only show this menu item for apps that open in an app window.
  const auto* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (provider->registrar_unsafe().GetAppUserDisplayMode(*app_id) ==
      web_app::mojom::UserDisplayMode::kBrowser) {
    return std::u16string();
  }

  const std::u16string short_name =
      base::UTF8ToUTF16(provider->registrar_unsafe().GetAppShortName(*app_id));
  return l10n_util::GetStringFUTF16(
      IDS_OPEN_IN_APP_WINDOW,
      ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
          short_name, GetLayoutConstant(APP_MENU_MAXIMUM_CHARACTER_LENGTH),
          gfx::CHARACTER_BREAK)));
}

bool IsPasswordManagerPage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.DomainIs(password_manager::kChromeUIPasswordManagerHost);
}

class ProfileSubMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  ProfileSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                      Profile* profile);
  ProfileSubMenuModel(const ProfileSubMenuModel&) = delete;
  ProfileSubMenuModel& operator=(const ProfileSubMenuModel&) = delete;
  ~ProfileSubMenuModel() override = default;

  const ui::ImageModel& avatar_image_model() const {
    return avatar_image_model_;
  }

  const std::u16string& profile_name() const { return profile_name_; }

  // Returns |next_menu_id_| and increments it by n. This allows for 'sharing'
  // command ids with the other variable sized menu, which also uses every other
  // int as an id.
  int GetAndIncrementNextMenuID();

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  bool BuildSyncSection();

  ui::ImageModel avatar_image_model_;
  std::u16string profile_name_;
  raw_ptr<Profile> profile_;
  raw_ptr<ui::SimpleMenuModel::Delegate> app_menu_model_delegate_;
  // ID of the next menu item.
  int next_other_profile_menu_id_;
  base::flat_map<int, base::FilePath> other_profiles_;
};

ProfileSubMenuModel::ProfileSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Profile* profile)
    : SimpleMenuModel(this),
      profile_(profile),
      app_menu_model_delegate_(delegate),
      next_other_profile_menu_id_(AppMenuModel::kMinOtherProfileCommandId) {
  const int avatar_icon_size =
      GetLayoutConstant(APP_MENU_PROFILE_ROW_AVATAR_ICON_SIZE);
  avatar_image_model_ = ui::ImageModel::FromVectorIcon(
      kAccountCircleChromeRefreshIcon, ui::kColorMenuIcon, avatar_icon_size);
  if (profile->IsIncognitoProfile()) {
    avatar_image_model_ = ui::ImageModel::FromVectorIcon(
        kIncognitoIcon, ui::kColorAvatarIconIncognito, avatar_icon_size);
    profile_name_ = l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE);
  } else if (profile->IsGuestSession()) {
    profile_name_ = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else {
    if (BuildSyncSection()) {
      AddSeparator(ui::NORMAL_SEPARATOR);
    }
    ProfileAttributesEntry* profile_attributes =
        GetProfileAttributesFromProfile(profile);
    // If the profile is being deleted, profile_attributes may be null.
    if (profile_attributes) {
      AccountInfo account_info = GetAccountInfoFromProfile(profile);
      gfx::Image avatar_image =
          account_info.IsEmpty()
              ? profile_attributes->GetAvatarIcon(avatar_icon_size)
              : account_info.account_image;
      // The avatar image can be empty if the account image hasn't been
      // fetched yet, if there is no image, or in tests.
      if (!avatar_image.IsEmpty()) {
        avatar_image_model_ =
            ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
                avatar_image, avatar_icon_size, avatar_icon_size,
                profiles::SHAPE_CIRCLE));
      }
      profile_name_ = GetProfileMenuDisplayName(profile_attributes);
    }
  }

  if (!profile->IsIncognitoProfile() && !profile->IsGuestSession()) {
    AddItemWithStringIdAndIcon(
        IDC_CUSTOMIZE_CHROME, IDS_CUSTOMIZE_CHROME,
        ui::ImageModel::FromVectorIcon(vector_icons::kEditChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  }

  AddItemWithIcon(
      IDC_CLOSE_PROFILE,
      l10n_util::GetPluralStringFUTF16(IDS_CLOSE_PROFILE,
                                       CountBrowsersFor(profile)),
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));

  if (HasUnconstentedProfile(profile) && !IsSyncPaused(profile) &&
      !profile->IsIncognitoProfile()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    const gfx::VectorIcon& manage_account_icon =
        vector_icons::kGoogleGLogoMonochromeIcon;
#else
    const gfx::VectorIcon& manage_account_icon =
        kAccountManageChromeRefreshIcon;
#endif
    AddItemWithStringIdAndIcon(
        IDC_MANAGE_GOOGLE_ACCOUNT, IDS_MANAGE_GOOGLE_ACCOUNT,
        ui::ImageModel::FromVectorIcon(manage_account_icon, ui::kColorMenuIcon,
                                       kDefaultIconSize));
  }

  if (!profile->IsIncognitoProfile() && !profile->IsGuestSession()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddTitle(l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
    auto profile_entries = GetAllOtherProfileEntriesForProfileSubMenu(profile);
    for (ProfileAttributesEntry* profile_entry : profile_entries) {
      std::u16string display_name = GetProfileMenuDisplayName(profile_entry);
      int menu_id = GetAndIncrementNextMenuID();
      AddItemWithIcon(menu_id,
                      ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
                          display_name,
                          GetLayoutConstant(APP_MENU_MAXIMUM_CHARACTER_LENGTH),
                          gfx::CHARACTER_BREAK)),
                      ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
                          profile_entry->GetAvatarIcon(), avatar_icon_size,
                          avatar_icon_size, profiles::SHAPE_CIRCLE)));
      other_profiles_.insert({menu_id, profile_entry->GetPath()});
    }

    if (profiles::IsGuestModeEnabled()) {
      AddItemWithStringIdAndIcon(
          IDC_OPEN_GUEST_PROFILE, IDS_OPEN_GUEST_PROFILE,
          ui::ImageModel::FromVectorIcon(
              vector_icons::kAccountCircleChromeRefreshIcon, ui::kColorMenuIcon,
              kDefaultIconSize));
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    if (profiles::IsProfileCreationAllowed()) {
      AddItemWithStringIdAndIcon(
          IDC_ADD_NEW_PROFILE, IDS_ADD_NEW_PROFILE,
          ui::ImageModel::FromVectorIcon(kAccountAddChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
    }
    AddItemWithStringIdAndIcon(
        IDC_MANAGE_CHROME_PROFILES, IDS_MANAGE_CHROME_PROFILES,
        ui::ImageModel::FromVectorIcon(kAccountManageChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  }
}

bool ProfileSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ProfileSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id >= AppMenuModel::kMinOtherProfileCommandId) {
    return true;
  }
  return app_menu_model_delegate_ &&
         app_menu_model_delegate_->IsCommandIdEnabled(command_id);
}

bool ProfileSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return app_menu_model_delegate_ &&
         app_menu_model_delegate_->GetAcceleratorForCommandId(command_id,
                                                              accelerator);
}

void ProfileSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (other_profiles_.find(command_id) != other_profiles_.end()) {
    return profiles::SwitchToProfile(other_profiles_[command_id],
                                     /*always_create=*/false);
  }
  if (app_menu_model_delegate_) {
    app_menu_model_delegate_->ExecuteCommand(command_id, event_flags);
  }
  return;
}

int ProfileSubMenuModel::GetAndIncrementNextMenuID() {
  const int current_id = next_other_profile_menu_id_;
  next_other_profile_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}

bool ProfileSubMenuModel::BuildSyncSection() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  if (!SyncServiceFactory::IsSyncAllowed(profile_)) {
    return false;
  }

  const AccountInfo account_info = GetAccountInfoFromProfile(profile_);

  const std::u16string signed_in_status =
      (IsSyncPaused(profile_) || account_info.IsEmpty())
          ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
          : l10n_util::GetStringFUTF16(IDS_PROFILE_ROW_SIGNED_IN_MESSAGE,
                                       {base::UTF8ToUTF16(account_info.email)});

  AddTitle(signed_in_status);
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  const bool is_sync_feature_enabled =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  // First, check for sync errors. They may exist even if sync-the-feature is
  // disabled and only sync-the-transport is running.
  const absl::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile_);
  if (error.has_value()) {
    if (error == AvatarSyncErrorType::kSyncPaused) {
      // If sync is paused the menu item will be specific to the paused error.
      AddItemWithStringIdAndIcon(IDC_SHOW_SIGNIN_WHEN_PAUSED,
                                 IDS_PROFILE_ROW_SIGN_IN_AGAIN,
                                 ui::ImageModel::FromVectorIcon(
                                     vector_icons::kSyncOffChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
    } else {
      // All remaining errors will have the same menu item.
      AddItemWithStringIdAndIcon(
          IDC_SHOW_SYNC_SETTINGS, IDS_PROFILE_ROW_SYNC_ERROR_MESSAGE,
          ui::ImageModel::FromVectorIcon(
              vector_icons::kSyncProblemChromeRefreshIcon, ui::kColorMenuIcon,
              kDefaultIconSize));
    }
    return true;
  }
  if (is_sync_feature_enabled) {
    AddItemWithStringIdAndIcon(
        IDC_SHOW_SYNC_SETTINGS, IDS_PROFILE_ROW_SYNC_IS_ON,
        ui::ImageModel::FromVectorIcon(vector_icons::kSyncChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  } else {
    AddItemWithStringIdAndIcon(
        IDC_TURN_ON_SYNC, IDS_PROFILE_ROW_TURN_ON_SYNC,
        ui::ImageModel::FromVectorIcon(vector_icons::kSyncOffChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  }
  return true;
}

class PasswordsAndAutofillSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit PasswordsAndAutofillSubMenuModel(
      ui::SimpleMenuModel::Delegate* delegate);
  PasswordsAndAutofillSubMenuModel(const PasswordsAndAutofillSubMenuModel&) =
      delete;
  PasswordsAndAutofillSubMenuModel& operator=(
      const PasswordsAndAutofillSubMenuModel&) = delete;
  ~PasswordsAndAutofillSubMenuModel() override = default;
};

PasswordsAndAutofillSubMenuModel::PasswordsAndAutofillSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringIdAndIcon(
      IDC_SHOW_PASSWORD_MANAGER, IDS_VIEW_PASSWORDS,
      ui::ImageModel::FromVectorIcon(kKeyChromeRefreshIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_PASSWORD_MANAGER).value(),
                         AppMenuModel::kPasswordManagerMenuItem);
  AddItemWithStringIdAndIcon(
      IDC_SHOW_PAYMENT_METHODS, IDS_PAYMENT_METHOD_SUBMENU_OPTION,
      ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
  AddItemWithStringIdAndIcon(
      IDC_SHOW_ADDRESSES, IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
      ui::ImageModel::FromVectorIcon(vector_icons::kLocationOnChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
}

class FindAndEditSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit FindAndEditSubMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  FindAndEditSubMenuModel(const FindAndEditSubMenuModel&) = delete;
  FindAndEditSubMenuModel& operator=(const FindAndEditSubMenuModel&) = delete;
  ~FindAndEditSubMenuModel() override = default;
};

FindAndEditSubMenuModel::FindAndEditSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringIdAndIcon(
      IDC_FIND, IDS_FIND,
      ui::ImageModel::FromVectorIcon(kSearchMenuIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(
      IDC_CUT, IDS_CUT,
      ui::ImageModel::FromVectorIcon(kCutMenuIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
  AddItemWithStringIdAndIcon(
      IDC_COPY, IDS_COPY,
      ui::ImageModel::FromVectorIcon(kCopyMenuIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
  AddItemWithStringIdAndIcon(
      IDC_PASTE, IDS_PASTE,
      ui::ImageModel::FromVectorIcon(kPasteMenuIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
}

class SaveAndShareSubMenuModel : public ui::SimpleMenuModel {
 public:
  SaveAndShareSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           Browser* browser);
  SaveAndShareSubMenuModel(const SaveAndShareSubMenuModel&) = delete;
  SaveAndShareSubMenuModel& operator=(const SaveAndShareSubMenuModel&) = delete;
  ~SaveAndShareSubMenuModel() override = default;
};

SaveAndShareSubMenuModel::SaveAndShareSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : SimpleMenuModel(delegate) {
  AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SAVE));
  AddItemWithStringIdAndIcon(
      IDC_SAVE_PAGE, IDS_SAVE_PAGE,
      ui::ImageModel::FromVectorIcon(kFileSaveChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (std::u16string install_item = GetInstallPWALabel(browser);
      !install_item.empty()) {
    AddItemWithIcon(
        IDC_INSTALL_PWA, install_item,
        ui::ImageModel::FromVectorIcon(kInstallDesktopChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  } else if (std::u16string open_item = GetOpenPWALabel(browser);
             !open_item.empty()) {
    AddItemWithIcon(
        IDC_OPEN_IN_PWA_WINDOW, open_item,
        ui::ImageModel::FromVectorIcon(kDesktopWindowsChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  }
  AddItemWithStringIdAndIcon(
      IDC_CREATE_SHORTCUT, IDS_ADD_TO_OS_LAUNCH_SURFACE,
      ui::ImageModel::FromVectorIcon(kDriveShortcutChromeRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
  if (!sharing_hub::SharingIsDisabledByPolicy(browser->profile()) ||
      media_router::MediaRouterEnabled(browser->profile())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SHARE));
    if (!sharing_hub::SharingIsDisabledByPolicy(browser->profile())) {
      AddItemWithStringIdAndIcon(
          IDC_COPY_URL, IDS_APP_MENU_COPY_LINK,
          ui::ImageModel::FromVectorIcon(kLinkChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
      AddItemWithStringIdAndIcon(
          IDC_SEND_TAB_TO_SELF, IDS_MENU_SEND_TAB_TO_SELF,
          ui::ImageModel::FromVectorIcon(kDevicesChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
      AddItemWithStringIdAndIcon(
          IDC_QRCODE_GENERATOR, IDS_APP_MENU_CREATE_QR_CODE,
          ui::ImageModel::FromVectorIcon(kQrCodeChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
    }
    if (media_router::MediaRouterEnabled(browser->profile())) {
      AddItemWithStringIdAndIcon(
          IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE,
          ui::ImageModel::FromVectorIcon(kCastChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
    }
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SetCommandIcon
void SetCommandIcon(ui::SimpleMenuModel* model,
                    int command_id,
                    const gfx::VectorIcon& vector_icon) {
  auto index = model->GetIndexOfCommandId(command_id);
  if (index) {
    model->SetIcon(index.value(), ui::ImageModel::FromVectorIcon(
                                      vector_icon, ui::kColorMenuIcon,
                                      ui::SimpleMenuModel::kDefaultIconSize));
  }
}

////////////////////////////////////////////////////////////////////////////////
// LogWrenchMenuAction
void LogWrenchMenuAction(AppMenuAction action_id) {
  base::UmaHistogramEnumeration("WrenchMenu.MenuAction", action_id,
                                LIMIT_MENU_ACTION);
}

////////////////////////////////////////////////////////////////////////////////
// HelpMenuModel
// Only used in branded builds.

class HelpMenuModel : public ui::SimpleMenuModel {
 public:
  HelpMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser)
      : SimpleMenuModel(delegate) {
    Build(browser);
  }

  HelpMenuModel(const HelpMenuModel&) = delete;
  HelpMenuModel& operator=(const HelpMenuModel&) = delete;

 private:
  void Build(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(OFFICIAL_BUILD)
    int help_string_id = IDS_GET_HELP;
#else
    int help_string_id = IDS_HELP_PAGE;
#endif
    AddItemWithStringId(IDC_ABOUT, IDS_ABOUT);
    if (features::IsChromeRefresh2023()) {
      SetCommandIcon(this, IDC_ABOUT, vector_icons::kInfoRefreshIcon);
    }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (whats_new::IsEnabled()) {
      AddItemWithStringId(IDC_CHROME_WHATS_NEW, IDS_CHROME_WHATS_NEW);
      if (features::IsChromeRefresh2023()) {
        SetCommandIcon(this, IDC_CHROME_WHATS_NEW, kReleaseAlertIcon);
      }
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    AddItemWithStringId(IDC_HELP_PAGE_VIA_MENU, help_string_id);
    if (browser_defaults::kShowHelpMenuItemIcon) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE_VIA_MENU).value(),
              ui::ImageModel::FromImage(rb.GetNativeImageNamed(IDR_HELP_MENU)));
    } else if (features::IsChromeRefresh2023()) {
      SetCommandIcon(this, IDC_HELP_PAGE_VIA_MENU, kHelpMenuIcon);
    }
    if (browser->profile()->GetPrefs()->GetBoolean(
            prefs::kUserFeedbackAllowed)) {
      AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);
      if (features::IsChromeRefresh2023()) {
        SetCommandIcon(this, IDC_FEEDBACK, kReportIcon);
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// ToolsMenuModel

ToolsMenuModel::ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                               Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ToolsMenuModel::~ToolsMenuModel() = default;

// More tools submenu is constructed as follows:
// - Page specific actions overflow (save page, adding to desktop).
// - Browser / OS level tools (extensions, task manager).
// - Reading mode.
// - Developer tools.
// - Option to enable profiling.
void ToolsMenuModel::Build(Browser* browser) {
  if (!features::IsChromeRefresh2023()) {
    AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);
    AddItemWithStringId(IDC_CREATE_SHORTCUT, IDS_ADD_TO_OS_LAUNCH_SURFACE);
  }
  AddItemWithStringId(IDC_NAME_WINDOW, IDS_NAME_WINDOW);
  if (commander::IsEnabled()) {
    AddItemWithStringId(IDC_TOGGLE_QUICK_COMMANDS, IDS_TOGGLE_QUICK_COMMANDS);
  }

  if (features::IsSidePanelPinningEnabled()) {
    AddItemWithStringId(IDC_SHOW_READING_MODE_SIDE_PANEL,
                        IDS_SHOW_READING_MODE_SIDE_PANEL);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_SHOW_READING_MODE_SIDE_PANEL).value(),
        kReadingModeMenuItem);
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  if (!features::IsChromeRefresh2023()) {
    AddItemWithStringId(IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA);
  }
  if (!features::IsExtensionMenuInRootAppMenu()) {
    AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);
  }
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    AddItemWithStringId(IDC_PERFORMANCE, IDS_SHOW_PERFORMANCE);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_PERFORMANCE).value(),
                           kPerformanceMenuItem);
  }
  if (chrome::CanOpenTaskManager())
    AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddItemWithStringId(IDC_TAKE_SCREENSHOT, IDS_TAKE_SCREENSHOT);
#endif
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_DEV_TOOLS, IDS_DEV_TOOLS);

  if (base::debug::IsProfilingSupported()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
  }
  if (features::IsChromeRefresh2023()) {
    if (IsChromeLabsEnabled()) {
      auto* profile = browser->profile();
      chrome_labs_model_ = std::make_unique<ChromeLabsModel>();
      UpdateChromeLabsNewBadgePrefs(profile, chrome_labs_model_.get());
      if (ShouldShowChromeLabsUI(chrome_labs_model_.get(), profile)) {
        BooleanPrefMember show_chrome_labs_item;
        show_chrome_labs_item.Init(
            chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy,
            profile->GetPrefs());
        if (show_chrome_labs_item.GetValue()) {
          AddSeparator(ui::NORMAL_SEPARATOR);
          AddItemWithStringIdAndIcon(IDC_SHOW_CHROME_LABS, IDS_CHROMELABS,
                                     ui::ImageModel::FromVectorIcon(
                                         kChromeLabsChromeRefreshIcon,
                                         ui::kColorMenuIcon, kDefaultIconSize));
          SetElementIdentifierAt(
              GetIndexOfCommandId(IDC_SHOW_CHROME_LABS).value(),
              kChromeLabsMenuItem);
        }
      }
    }
    SetCommandIcon(this, IDC_NAME_WINDOW, kNameWindowIcon);
    SetCommandIcon(this, IDC_TOGGLE_QUICK_COMMANDS, kQuickCommandsIcon);
    SetCommandIcon(this, IDC_SHOW_READING_MODE_SIDE_PANEL,
                   kMenuBookChromeRefreshIcon);
    SetCommandIcon(this, IDC_PERFORMANCE, kPerformanceIcon);
    SetCommandIcon(this, IDC_TASK_MANAGER, kTaskManagerIcon);
    SetCommandIcon(this, IDC_DEV_TOOLS, kDeveloperToolsIcon);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionsMenuModel

ExtensionsMenuModel::ExtensionsMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ExtensionsMenuModel::~ExtensionsMenuModel() = default;

// Extensions (sub)menu is constructed as follows:
// - An overflow with two items:
//   - An item to manage extensions at chrome://extensions
//   - An item to visit the Chrome Web Store
void ExtensionsMenuModel::Build(Browser* browser) {
  AddItemWithStringId(IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
                      IDS_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS_ITEM);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS).value(),
      kManageExtensionsMenuItem);
  AddItemWithStringId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                      IDS_EXTENSIONS_SUBMENU_CHROME_WEBSTORE_ITEM);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE)
          .value(),
      kVisitChromeWebStoreMenuItem);
  if (features::IsChromeRefresh2023()) {
    SetCommandIcon(this, IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
                   vector_icons::kExtensionChromeRefreshIcon);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    SetCommandIcon(this, IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                   vector_icons::kGoogleChromeWebstoreIcon);
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppMenuModel

AppMenuModel::AppMenuModel(ui::AcceleratorProvider* provider,
                           Browser* browser,
                           AppMenuIconController* app_menu_icon_controller,
                           AlertMenuItem alert_item)
    : ui::SimpleMenuModel(this),
      uma_action_recorded_(false),
      provider_(provider),
      browser_(browser),
      app_menu_icon_controller_(app_menu_icon_controller),
      alert_item_(alert_item) {
  DCHECK(browser_);
}

AppMenuModel::~AppMenuModel() = default;

void AppMenuModel::SetHighlightedIdentifier(
    ui::ElementIdentifier highlighted_menu_identifier) {
  highlighted_menu_identifier_ = highlighted_menu_identifier;
}

void AppMenuModel::Init() {
  Build();

#if BUILDFLAG(IS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (local_state) {
    local_state_pref_change_registrar_.Init(local_state);
    local_state_pref_change_registrar_.Add(
        policy::policy_prefs::kSystemFeaturesDisableList,
        base::BindRepeating(&AppMenuModel::UpdateSettingsItemState,
                            base::Unretained(this)));
    UpdateSettingsItemState();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool AppMenuModel::DoesCommandIdDismissMenu(int command_id) const {
  return command_id != IDC_ZOOM_MINUS && command_id != IDC_ZOOM_PLUS;
}

void AppMenuModel::ExecuteCommand(int command_id, int event_flags) {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  if (command_id == IDC_VIEW_PASSWORDS) {
    browser()->profile()->GetPrefs()->SetBoolean(
        password_manager::prefs::kPasswordsPrefWithNewLabelUsed, true);
  }

  LogMenuMetrics(command_id);
  chrome::ExecuteCommand(browser_, command_id);
}

void AppMenuModel::LogSafetyHubInteractionMetrics(
    absl::optional<safety_hub::SafetyHubModuleType> expected_module) {
  // TODO(crbug.com/1443466): Remove when the service is only created when the
  // feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return;
  }
  auto* const safety_hub_menu_notification_service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(
          browser_->profile());
  if (!safety_hub_menu_notification_service) {
    return;
  }
  absl::optional<safety_hub::SafetyHubModuleType> sh_module =
      safety_hub_menu_notification_service->GetModuleOfActiveNotification();
  if (sh_module.has_value() && (!expected_module.has_value() ||
                                expected_module.value() == sh_module.value())) {
    base::UmaHistogramEnumeration("Settings.SafetyHub.Interaction",
                                  safety_hub::SafetyHubSurfaces::kThreeDotMenu);
    base::UmaHistogramEnumeration(
        "Settings.SafetyHub.EntryPointInteraction",
        safety_hub::SafetyHubEntryPoint::kMenuNotifications);
    base::UmaHistogramEnumeration("Settings.SafetyHub.MenuNotificationClicked",
                                  sh_module.value());
  }
}

void AppMenuModel::LogMenuMetrics(int command_id) {
  base::TimeDelta delta = timer_.Elapsed();

  switch (command_id) {
    case IDC_UPGRADE_DIALOG:
      LogMenuAction(MENU_ACTION_UPGRADE_DIALOG);
      break;
    case IDC_SHOW_PASSWORD_CHECKUP:
      if (!uma_action_recorded_) {
        LogSafetyHubInteractionMetrics(
            safety_hub::SafetyHubModuleType::PASSWORDS);
      }
      LogMenuAction(MENU_ACTION_SHOW_PASSWORD_CHECKUP);
      break;
    case IDC_OPEN_SAFETY_HUB:
      if (!uma_action_recorded_) {
        // Multiple Safety Hub module types can result in opening Safety Hub UI.
        LogSafetyHubInteractionMetrics();
      }
      LogMenuAction(MENU_ACTION_SHOW_SAFETY_HUB);
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case IDC_LACROS_DATA_MIGRATION:
      LogMenuAction(MENU_ACTION_LACROS_DATA_MIGRATION);
      break;
#endif
    case IDC_NEW_TAB:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewTab", delta);
      LogMenuAction(MENU_ACTION_NEW_TAB);
      break;
    case IDC_NEW_WINDOW:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewWindow",
                                      delta);
      LogMenuAction(MENU_ACTION_NEW_WINDOW);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.NewIncognitoWindow", delta);
      }
      LogMenuAction(MENU_ACTION_NEW_INCOGNITO_WINDOW);
      break;

    // Bookmarks sub menu.
    case IDC_SHOW_BOOKMARK_BAR:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowBookmarkBar",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_BAR);
      break;
    case IDC_SHOW_BOOKMARK_SIDE_PANEL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowBookmarkSidePanel", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_SIDE_PANEL);
      // Close IPH for side panel menu, if shown.
      browser()->window()->NotifyFeatureEngagementEvent(
          feature_engagement::events::kSidePanelFromMenuShown);
      break;
    case IDC_SHOW_BOOKMARK_MANAGER:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowBookmarkMgr",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_MANAGER);
      break;
    case IDC_IMPORT_SETTINGS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ImportSettings",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_IMPORT_SETTINGS);
      break;
    case IDC_BOOKMARK_THIS_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BookmarkPage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_THIS_TAB);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BookmarkAllTabs",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_ALL_TABS);
      break;
      // Search companion.
    case IDC_SHOW_SEARCH_COMPANION:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowSearchCompanion", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_SEARCH_COMPANION);
      break;
    // Extensions menu.
    case IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS:
      CHECK(features::IsExtensionMenuInRootAppMenu());
      // Logging the original histograms for experiment comparison purposes.
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageExtensions", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_EXTENSIONS);
      break;
    case IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE:
      CHECK(features::IsExtensionMenuInRootAppMenu());
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.VisitChromeWebStore", delta);
      }
      LogMenuAction(MENU_ACTION_VISIT_CHROME_WEB_STORE);
      break;
    // Recent tabs menu.
    case IDC_RESTORE_TAB:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.RestoreTab",
                                      delta);
      LogMenuAction(MENU_ACTION_RESTORE_TAB);
      break;
    case IDC_OPEN_RECENT_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenRecentTab",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_RECENT_TAB);
      break;
    case IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.LoginForDeviceTabs", delta);
      }
      LogMenuAction(MENU_ACTION_RECENT_TABS_LOGIN_FOR_DEVICE_TABS);
      break;
    case IDC_DISTILL_PAGE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DistillPage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DISTILL_PAGE);
      if (dom_distiller::url_utils::IsDistilledPage(
              browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL())) {
        dom_distiller::UMAHelper::RecordReaderModeExit(
            dom_distiller::UMAHelper::ReaderModeEntryPoint::kMenuOption);
      } else {
        dom_distiller::UMAHelper::RecordReaderModeEntry(
            dom_distiller::UMAHelper::ReaderModeEntryPoint::kMenuOption);
      }
      break;
    case IDC_FIND:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Find", delta);
      LogMenuAction(MENU_ACTION_FIND);
      break;
    case IDC_PRINT:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Print", delta);
      LogMenuAction(MENU_ACTION_PRINT);
      break;

    case IDC_SHOW_TRANSLATE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowTranslate",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_TRANSLATE);
      break;

    // Edit menu.
    case IDC_CUT:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Cut", delta);
      LogMenuAction(MENU_ACTION_CUT);
      break;
    case IDC_COPY:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Copy", delta);
      LogMenuAction(MENU_ACTION_COPY);
      break;
    case IDC_PASTE:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Paste", delta);
      LogMenuAction(MENU_ACTION_PASTE);
      break;

    // Save and share menu.
    case IDC_SAVE_PAGE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.SavePage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SAVE_PAGE);
      break;
    case IDC_INSTALL_PWA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.InstallPwa",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_INSTALL_PWA);
      break;
    case IDC_OPEN_IN_PWA_WINDOW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenInPwaWindow",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_IN_PWA_WINDOW);
      break;
    case IDC_CREATE_SHORTCUT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CreateHostedApp",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CREATE_HOSTED_APP);
      break;
    case IDC_COPY_URL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CopyUrl", delta);
      }
      LogMenuAction(MENU_ACTION_COPY_URL);
      break;
    case IDC_SEND_TAB_TO_SELF:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.SendToDevices",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SEND_TO_DEVICES);
      break;
    case IDC_QRCODE_GENERATOR:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CreateQrCode",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CREATE_QR_CODE);
      break;
    case IDC_ROUTE_MEDIA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Cast", delta);
      }
      LogMenuAction(MENU_ACTION_CAST);
      break;

    // Tools menu.
    case IDC_MANAGE_EXTENSIONS:
      if (!uma_action_recorded_) {
        // TODO(crbug.com/1443466): Use a callback instead to log the metrics to
        // reduce coupling with Safety Hub notification service.
        // See crrev.com/c/5012653/comments/4f038126_bb7cb0fe for more details.
        if (features::IsExtensionMenuInRootAppMenu()) {
          LogSafetyHubInteractionMetrics();
        } else {
          // The command can originate from either Safety Hub notification or
          // extension menu.
          LogSafetyHubInteractionMetrics(
              safety_hub::SafetyHubModuleType::EXTENSIONS);
        }
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageExtensions", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_EXTENSIONS);
      break;
    case IDC_TASK_MANAGER:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.TaskManager",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_TASK_MANAGER);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ClearBrowsingData", delta);
      }
      LogMenuAction(MENU_ACTION_CLEAR_BROWSING_DATA);
      break;
    case IDC_VIEW_SOURCE:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ViewSource",
                                      delta);
      LogMenuAction(MENU_ACTION_VIEW_SOURCE);
      break;
    case IDC_DEV_TOOLS:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevTools",
                                      delta);
      LogMenuAction(MENU_ACTION_DEV_TOOLS);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevToolsConsole",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_CONSOLE);
      break;
    case IDC_DEV_TOOLS_DEVICES:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevToolsDevices",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_DEVICES);
      break;
    case IDC_PROFILING_ENABLED:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ProfilingEnabled", delta);
      }
      LogMenuAction(MENU_ACTION_PROFILING_ENABLED);
      break;
    case IDC_SHOW_CHROME_LABS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowChromeLabs",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_CHROME_LABS);
      break;
    case IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowHistoryClustersSidePanel", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_HISTORY_CLUSTER_SIDE_PANEL);
      break;
    case IDC_SHOW_READING_MODE_SIDE_PANEL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowReadingModeSidePanel", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_READING_MODE_SIDE_PANEL);
      // Close IPH for side panel menu, if shown.
      browser()->window()->NotifyFeatureEngagementEvent(
          feature_engagement::events::kSidePanelFromMenuShown);
      break;

    // Zoom menu
    case IDC_ZOOM_MINUS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ZoomMinus",
                                      delta);
        LogMenuAction(MENU_ACTION_ZOOM_MINUS);
      }
      break;
    case IDC_ZOOM_PLUS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ZoomPlus",
                                      delta);
        LogMenuAction(MENU_ACTION_ZOOM_PLUS);
      }
      break;
    case IDC_FULLSCREEN:
      base::RecordAction(UserMetricsAction("EnterFullScreenWithWrenchMenu"));

      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.EnterFullScreen",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_FULLSCREEN);
      break;
    case IDC_SHOW_HISTORY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowHistory",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_HISTORY);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowDownloads",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_DOWNLOADS);
      base::UmaHistogramEnumeration(
          "Download.OpenDownloadsFromMenu.PerProfileType",
          profile_metrics::GetBrowserProfileType(browser_->profile()));
      break;
    case IDC_OPTIONS:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Settings",
                                      delta);
      LogMenuAction(MENU_ACTION_OPTIONS);
      base::UmaHistogramEnumeration(
          "Settings.OpenSettingsFromMenu.PerProfileType",
          profile_metrics::GetBrowserProfileType(browser_->profile()));
      break;
    case IDC_ABOUT:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.About", delta);
      LogMenuAction(MENU_ACTION_ABOUT);
      break;
    // Help menu.
    case IDC_HELP_PAGE_VIA_MENU:
      base::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.HelpPage",
                                      delta);
      LogMenuAction(MENU_ACTION_HELP_PAGE_VIA_MENU);
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_SHOW_BETA_FORUM:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BetaForum",
                                      delta);
      LogMenuAction(MENU_ACTION_BETA_FORUM);
      break;
    case IDC_FEEDBACK:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Feedback",
                                      delta);
      LogMenuAction(MENU_ACTION_FEEDBACK);
      break;
    case IDC_CHROME_TIPS:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ChromeTips",
                                      delta);
      LogMenuAction(MENU_ACTION_CHROME_TIPS);
      break;
    case IDC_CHROME_WHATS_NEW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ChromeWhatsNew",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CHROME_WHATS_NEW);
      break;
#endif

    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.RequestTabletSite", delta);
      }
      LogMenuAction(MENU_ACTION_TOGGLE_REQUEST_TABLET_SITE);
      break;
    case IDC_EXIT:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Exit", delta);
      LogMenuAction(MENU_ACTION_EXIT);
      break;

    // Hosted App menu.
    case IDC_OPEN_IN_CHROME:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenInChrome",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_IN_CHROME);
      break;
    case IDC_WEB_APP_MENU_APP_INFO:
      if (!uma_action_recorded_)
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.AppInfo", delta);
      LogMenuAction(MENU_ACTION_APP_INFO);
      break;
    case IDC_VIEW_PASSWORDS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.PasswordManager",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_PASSWORD_MANAGER);
      break;

      // Profile submenu.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    case IDC_CUSTOMIZE_CHROME:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CustomizeChrome",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CUSTOMIZE_CHROME);
      break;
    case IDC_CLOSE_PROFILE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CloseProfile",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CLOSE_PROFILE);
      break;
    case IDC_MANAGE_GOOGLE_ACCOUNT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageGoogleAccount", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_GOOGLE_ACCOUNT);
      break;
    case IDC_SHOW_SYNC_SETTINGS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowSyncSettings", delta);
      }
      LogMenuAction(MENU_SHOW_SYNC_SETTINGS);
      break;
    case IDC_TURN_ON_SYNC:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowTurnOnSync",
                                      delta);
      }
      LogMenuAction(MENU_TURN_ON_SYNC);
      break;
    case IDC_SHOW_SIGNIN_WHEN_PAUSED:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowSigninWhenPaused", delta);
      }
      LogMenuAction(MENU_SHOW_SIGNIN_WHEN_PAUSED);
      break;
    case IDC_OPEN_GUEST_PROFILE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.OpenGuestProfile", delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_GUEST_PROFILE);
      break;
    case IDC_ADD_NEW_PROFILE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.AddNewProfile",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_ADD_NEW_PROFILE);
      break;
    case IDC_MANAGE_CHROME_PROFILES:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageChromeProfiles", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_CHROME_PROFILES);
      break;
#endif

    // Reading list submenu.
    case IDC_READING_LIST_MENU_ADD_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ReadingListAddTab", delta);
      }
      LogMenuAction(MENU_ACTION_READING_LIST_ADD_TAB);
      break;
    case IDC_READING_LIST_MENU_SHOW_UI:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ReadingListShowUi", delta);
      }
      LogMenuAction(MENU_ACTION_READING_LIST_SHOW_UI);
      break;

    // Password autofill submenu.
    case IDC_SHOW_PASSWORD_MANAGER:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowPasswordManager", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_PASSWORD_MANAGER);
      break;
    case IDC_SHOW_PAYMENT_METHODS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowPaymentMethods", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_PAYMENT_METHODS);
      break;
    case IDC_SHOW_ADDRESSES:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowAddresses",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_ADDRESSES);
      break;
    case IDC_PERFORMANCE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowPerformanceSettings", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_PERFORMANCE_SETTINGS);
      break;
    default: {
      if (IsOtherProfileCommand(command_id)) {
        if (!uma_action_recorded_) {
          base::UmaHistogramMediumTimes(
              "WrenchMenu.TimeToAction.SwitchToAnotherProfile", delta);
        }
        LogMenuAction(MENU_ACTION_SWITCH_TO_ANOTHER_PROFILE);
      }
      break;
    }
  }

  if (!uma_action_recorded_) {
    base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction", delta);
    uma_action_recorded_ = true;
  }
}

bool AppMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(
        bookmarks::prefs::kShowBookmarkBar);
  }
  if (command_id == IDC_PROFILING_ENABLED)
    return content::Profiling::BeingProfiled();
  if (command_id == IDC_TOGGLE_REQUEST_TABLET_SITE)
    return chrome::IsRequestingTabletSite(browser_);

  return false;
}

bool AppMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error)
    return true;

  switch (command_id) {
    case IDC_NEW_INCOGNITO_WINDOW:
      return IncognitoModePrefs::IsIncognitoAllowed(browser_->profile());
    default:
      return chrome::IsCommandEnabled(browser_, command_id);
  }
}

bool AppMenuModel::IsCommandIdAlerted(int command_id) const {
  if (command_id == IDC_VIEW_PASSWORDS ||
      command_id == IDC_SHOW_PASSWORD_MANAGER) {
    return alert_item_ == AlertMenuItem::kPasswordManager;
  }

  return false;
}

bool AppMenuModel::IsElementIdAlerted(ui::ElementIdentifier element_id) const {
  return highlighted_menu_identifier_ == element_id;
}

bool AppMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

void AppMenuModel::LogMenuAction(AppMenuAction action_id) {
  LogWrenchMenuAction(action_id);
}

// Note: When adding new menu items please place under an appropriate section.
// Menu is organised as follows:
// - Extension toolbar overflow.
// - Global browser errors and warnings.
// - Tabs and windows.
// - Places previously been e.g. History, bookmarks, recent tabs.
// - Page actions e.g. zoom, edit, find, print.
// - Learn about the browser and global customisation e.g. settings, help.
// - Browser relaunch, quit.
void AppMenuModel::Build() {
  // Build (and, by extension, Init) should only be called once.
  DCHECK_EQ(0u, GetItemCount());

  bool need_separator = false;
  if (app_menu_icon_controller_ &&
      app_menu_icon_controller_->GetTypeAndSeverity().type ==
          AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    const auto update_icon =
        features::IsChromeRefresh2023()
            ? ui::ImageModel::FromVectorIcon(
                  kBrowserToolsUpdateChromeRefreshIcon, ui::kColorMenuIcon,
                  kDefaultIconSize)
            : ui::ImageModel::FromVectorIcon(
                  kBrowserToolsUpdateIcon,
                  app_menu_icon_controller_->GetIconColor(absl::nullopt));
    if (browser_defaults::kShowUpgradeMenuItem) {
      AddItemWithIcon(IDC_UPGRADE_DIALOG, GetUpgradeDialogMenuItemName(),
                      update_icon);
      need_separator = true;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    AddItemWithIcon(IDC_LACROS_DATA_MIGRATION,
                    GetLacrosDataMigrationMenuItemName(), update_icon);
    need_separator = true;
#endif
  }

  if (AddSafetyHubMenuItem() || AddGlobalErrorMenuItems() || need_separator) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  AddItemWithStringId(IDC_NEW_TAB, browser_->profile()->IsIncognitoProfile()
                                       ? IDS_NEW_INCOGNITO_TAB
                                       : IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);

  // This menu item is not visible in Guest Mode. If incognito mode is not
  // available, it will be shown in disabled state. (crbug.com/1100791)
  if (!browser_->profile()->IsGuestSession()) {
    AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_NEW_INCOGNITO_WINDOW).value(),
        kIncognitoMenuItem);
  }

  AddSeparator(ui::NORMAL_SEPARATOR);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsChromeRefresh2023()) {
    sub_menus_.push_back(
        std::make_unique<ProfileSubMenuModel>(this, browser()->profile()));
    auto* const profile_submenu_model =
        static_cast<ProfileSubMenuModel*>(sub_menus_.back().get());
    AddSubMenu(IDC_PROFILE_MENU_IN_APP_MENU,
               profile_submenu_model->profile_name(), profile_submenu_model);
    SetIcon(GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value(),
            profile_submenu_model->avatar_image_model());
    AddSeparator(ui::SPACING_SEPARATOR);
  }
#endif

  if (!browser_->profile()->IsGuestSession() &&
      features::IsChromeRefresh2023()) {
    sub_menus_.push_back(
        std::make_unique<PasswordsAndAutofillSubMenuModel>(this));
    AddSubMenuWithStringId(IDC_PASSWORDS_AND_AUTOFILL_MENU,
                           IDS_PASSWORDS_AND_AUTOFILL_MENU,
                           sub_menus_.back().get());
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_PASSWORDS_AND_AUTOFILL_MENU).value(),
        kPasswordAndAutofillMenuItem);
  }

  if (!browser_->profile()->IsOffTheRecord()) {
    auto recent_tabs_sub_menu =
        std::make_unique<RecentTabsSubMenuModel>(provider_, browser_);
    recent_tabs_sub_menu->RegisterLogMenuMetricsCallback(base::BindRepeating(
        &AppMenuModel::LogMenuMetrics, base::Unretained(this)));
    sub_menus_.push_back(std::move(recent_tabs_sub_menu));
    AddSubMenuWithStringId(IDC_RECENT_TABS_MENU, IDS_HISTORY_MENU,
                           sub_menus_.back().get());
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_RECENT_TABS_MENU).value(),
                           kHistoryMenuItem);
  }
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_DOWNLOADS).value(),
                         kDownloadsMenuItem);
  if (!browser_->profile()->IsGuestSession()) {
    bookmark_sub_menu_model_ =
        std::make_unique<BookmarkSubMenuModel>(this, browser_);

    AddSubMenuWithStringId(IDC_BOOKMARKS_MENU,
                           features::IsChromeRefresh2023()
                               ? IDS_BOOKMARKS_AND_LISTS_MENU
                               : IDS_BOOKMARKS_MENU,
                           bookmark_sub_menu_model_.get());
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_BOOKMARKS_MENU).value(),
                           kBookmarksMenuItem);
  }
  WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!browser_->profile()->IsOffTheRecord() && web_contents &&
      !IsPasswordManagerPage(web_contents->GetURL()) &&
      !features::IsChromeRefresh2023()) {
    AddItemWithStringId(IDC_VIEW_PASSWORDS, IDS_VIEW_PASSWORDS);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_VIEW_PASSWORDS).value(),
                           kPasswordManagerMenuItem);
    if (!browser_->profile()->GetPrefs()->GetBoolean(
            password_manager::prefs::kPasswordsPrefWithNewLabelUsed)) {
      SetIsNewFeatureAt(GetIndexOfCommandId(IDC_VIEW_PASSWORDS).value(), true);
    }
  }

  if (features::IsExtensionMenuInRootAppMenu()) {
    // Extensions sub menu.
    sub_menus_.push_back(std::make_unique<ExtensionsMenuModel>(this, browser_));
    AddSubMenuWithStringId(IDC_EXTENSIONS_SUBMENU, IDS_EXTENSIONS_SUBMENU,
                           sub_menus_.back().get());
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU).value(),
                           kExtensionsMenuItem);
    if (features::IsChromeRefresh2023()) {
      SetCommandIcon(this, IDC_EXTENSIONS_SUBMENU,
                     vector_icons::kExtensionChromeRefreshIcon);
    }
  }
  if (features::IsChromeRefresh2023()) {
    AddItemWithStringIdAndIcon(
        IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA,
        ui::ImageModel::FromVectorIcon(kTrashCanRefreshIcon, ui::kColorMenuIcon,
                                       kDefaultIconSize));
  }

  AddSeparator(features::IsChromeRefresh2023() ? ui::NORMAL_SEPARATOR
                                               : ui::LOWER_SEPARATOR);
  CreateZoomMenu();
  AddSeparator(features::IsChromeRefresh2023() ? ui::NORMAL_SEPARATOR
                                               : ui::UPPER_SEPARATOR);

  AddItemWithStringId(IDC_PRINT, IDS_PRINT);

  if (features::IsChromeRefresh2023()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (companion::IsCompanionFeatureEnabled()) {
      AddItemWithStringId(IDC_SHOW_SEARCH_COMPANION, IDS_SHOW_SEARCH_COMPANION);
      SetElementIdentifierAt(
          GetIndexOfCommandId(IDC_SHOW_SEARCH_COMPANION).value(),
          kShowSearchCompanion);
    }
#endif
    if (TabOrganizationUtils::GetInstance()->IsEnabled(browser_->profile())) {
      auto* const tab_organization_service =
          TabOrganizationServiceFactory::GetForProfile(browser_->profile());
      if (tab_organization_service) {
        AddItemWithStringId(IDC_ORGANIZE_TABS, IDS_TAB_ORGANIZE_MENU);
        SetIsNewFeatureAt(GetIndexOfCommandId(IDC_ORGANIZE_TABS).value(), true);
      }
    }

    if (base::FeatureList::IsEnabled(
            performance_manager::features::kPerformanceControlsSidePanel)) {
      AddItemWithStringId(IDC_PERFORMANCE, IDS_SHOW_PERFORMANCE);
      SetElementIdentifierAt(GetIndexOfCommandId(IDC_PERFORMANCE).value(),
                             kPerformanceMenuItem);
    }

    AddItemWithStringId(IDC_SHOW_TRANSLATE, IDS_SHOW_TRANSLATE);

    CreateFindAndEditSubMenu();

    sub_menus_.push_back(
        std::make_unique<SaveAndShareSubMenuModel>(this, browser_));
    AddSubMenuWithStringId(IDC_SAVE_AND_SHARE_MENU, IDS_SAVE_AND_SHARE_MENU,
                           sub_menus_.back().get());
  } else {
    if (base::FeatureList::IsEnabled(
            performance_manager::features::kPerformanceControlsSidePanel)) {
      AddItemWithStringId(IDC_PERFORMANCE, IDS_SHOW_PERFORMANCE);
      SetElementIdentifierAt(GetIndexOfCommandId(IDC_PERFORMANCE).value(),
                             kPerformanceMenuItem);
    }

    if (media_router::MediaRouterEnabled(browser()->profile())) {
      AddItemWithStringId(IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
    }

    AddItemWithStringId(IDC_FIND, IDS_FIND);

    if (std::u16string install_item = GetInstallPWALabel(browser_);
        !install_item.empty()) {
      AddItem(IDC_INSTALL_PWA, install_item);
    } else if (std::u16string open_item = GetOpenPWALabel(browser_);
               !open_item.empty()) {
      AddItem(IDC_OPEN_IN_PWA_WINDOW, open_item);
    }
  }

  if (dom_distiller::IsDomDistillerEnabled() &&
      browser()->tab_strip_model()->GetActiveWebContents()) {
    // Only show the reader mode toggle when it will do something.
    if (dom_distiller::url_utils::IsDistilledPage(
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL())) {
      // Show the menu option if we are on a distilled page.
      AddItemWithStringId(IDC_DISTILL_PAGE, IDS_EXIT_DISTILLED_PAGE);
    } else if (dom_distiller::ShowReaderModeOption(
                   browser_->profile()->GetPrefs())) {
      // Show the menu option if the page is distillable.
      absl::optional<dom_distiller::DistillabilityResult> distillability =
          dom_distiller::GetLatestResult(
              browser()->tab_strip_model()->GetActiveWebContents());
      if (distillability && distillability.value().is_distillable)
        AddItemWithStringId(IDC_DISTILL_PAGE, IDS_DISTILL_PAGE);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Always show this option if we're in tablet mode on Chrome OS.
  if (display::Screen::GetScreen()->InTabletMode()) {
    if (features::IsChromeRefresh2023()) {
      AddItemWithStringIdAndIcon(
          IDC_TOGGLE_REQUEST_TABLET_SITE, IDS_TOGGLE_REQUEST_TABLET_SITE,
          ui::ImageModel::FromVectorIcon(
              chrome::IsRequestingTabletSite(browser_)
                  ? kRequestMobileSiteCheckedIcon
                  : kRequestMobileSiteUncheckedIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
    } else {
      AddCheckItemWithStringId(IDC_TOGGLE_REQUEST_TABLET_SITE,
                               IDS_TOGGLE_REQUEST_TABLET_SITE);
    }
  }
#endif

  sub_menus_.push_back(std::make_unique<ToolsMenuModel>(this, browser_));
  AddSubMenuWithStringId(IDC_MORE_TOOLS_MENU, IDS_MORE_TOOLS_MENU,
                         sub_menus_.back().get());
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_MORE_TOOLS_MENU).value(),
                         kMoreToolsMenuItem);

  if (!features::IsChromeRefresh2023()) {
    AddSeparator(ui::LOWER_SEPARATOR);
    CreateCutCopyPasteMenu();
    AddSeparator(ui::UPPER_SEPARATOR);
  }

  if (!features::IsChromeRefresh2023()) {
    AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
  } else {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
// The help submenu is only displayed on official Chrome builds. As the
// 'About' item has been moved to this submenu, it's reinstated here for
// Chromium builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  sub_menus_.push_back(std::make_unique<HelpMenuModel>(this, browser_));
  AddSubMenuWithStringId(IDC_HELP_MENU, IDS_HELP_MENU, sub_menus_.back().get());
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#else
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#endif
#endif

  if (features::IsChromeRefresh2023()) {
    AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
  }

  if (browser_defaults::kShowExitMenuItem) {
    if (!features::IsChromeRefresh2023()) {
      AddSeparator(ui::NORMAL_SEPARATOR);
    }
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  }

  // On Chrome OS, similar UI is displayed in the system tray menu, instead of
  // this menu.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (chrome::ShouldDisplayManagedUi(browser_->profile())) {
    AddSeparator(features::IsChromeRefresh2023() ? ui::NORMAL_SEPARATOR
                                                 : ui::LOWER_SEPARATOR);
    if (!features::IsChromeRefresh2023()) {
      const int kIconSize = 18;
      AddHighlightedItemWithIcon(
          IDC_SHOW_MANAGEMENT_PAGE,
          chrome::GetManagedUiMenuItemLabel(browser_->profile()),
          ui::ImageModel::FromVectorIcon(
              chrome::GetManagedUiIcon(browser_->profile()),
              ui::kColorMenuItemForegroundHighlighted, kIconSize));
    } else {
      AddItemWithIcon(IDC_SHOW_MANAGEMENT_PAGE,
                      chrome::GetManagedUiMenuItemLabel(browser_->profile()),
                      ui::ImageModel::FromVectorIcon(
                          chrome::GetManagedUiIcon(browser_->profile()),
                          ui::kColorMenuIcon, kDefaultIconSize));
    }

    SetAccessibleNameAt(
        GetIndexOfCommandId(IDC_SHOW_MANAGEMENT_PAGE).value(),
        chrome::GetManagedUiMenuItemTooltip(browser_->profile()));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  if (features::IsChromeRefresh2023()) {
    SetCommandIcon(this, IDC_NEW_TAB, kNewTabRefreshIcon);
    SetCommandIcon(this, IDC_NEW_WINDOW, kNewWindowIcon);
    SetCommandIcon(this, IDC_NEW_INCOGNITO_WINDOW, kIncognitoRefreshMenuIcon);
    SetCommandIcon(this, IDC_RECENT_TABS_MENU, kHistoryIcon);
    SetCommandIcon(this, IDC_SHOW_DOWNLOADS, kDownloadMenuIcon);
    SetCommandIcon(this, IDC_BOOKMARKS_MENU, kBookmarksListsMenuIcon);
    SetCommandIcon(this, IDC_VIEW_PASSWORDS, kKeyOpenChromeRefreshIcon);
    SetCommandIcon(this, IDC_ZOOM_MENU, kZoomInIcon);
    SetCommandIcon(this, IDC_PRINT, kPrintMenuIcon);
    SetCommandIcon(this, IDC_ORGANIZE_TABS, kAutoTabGroupsIcon);
    SetCommandIcon(this, IDC_SHOW_TRANSLATE, kTranslateIcon);
    SetCommandIcon(this, IDC_FIND_AND_EDIT_MENU, kSearchMenuIcon);
    SetCommandIcon(this, IDC_SAVE_AND_SHARE_MENU, kFileSaveChromeRefreshIcon);
    SetCommandIcon(this, IDC_PASSWORDS_AND_AUTOFILL_MENU,
                   kKeyOpenChromeRefreshIcon);
    SetCommandIcon(this, IDC_MORE_TOOLS_MENU, kMoreToolsMenuIcon);
    SetCommandIcon(this, IDC_OPTIONS, kSettingsMenuIcon);
    SetCommandIcon(this, IDC_PERFORMANCE, kPerformanceIcon);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    SetCommandIcon(this, IDC_HELP_MENU, kHelpMenuIcon);
    SetCommandIcon(this, IDC_SHOW_SEARCH_COMPANION,
                   vector_icons::kGoogleGLogoMonochromeIcon);
#endif
    SetCommandIcon(this, IDC_EXIT, kExitMenuIcon);
  }

  uma_action_recorded_ = false;
}

void AppMenuModel::CreateCutCopyPasteMenu() {
  edit_menu_item_model_ =
      std::make_unique<ui::ButtonMenuItemModel>(IDS_EDIT, this);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_CUT, IDS_CUT);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_COPY, IDS_COPY);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_PASTE, IDS_PASTE);
  AddButtonItem(IDC_EDIT_MENU, edit_menu_item_model_.get());
}

void AppMenuModel::CreateFindAndEditSubMenu() {
  sub_menus_.push_back(std::make_unique<FindAndEditSubMenuModel>(this));
  AddSubMenuWithStringId(IDC_FIND_AND_EDIT_MENU, IDS_FIND_AND_EDIT_MENU,
                         sub_menus_.back().get());
}

void AppMenuModel::CreateZoomMenu() {
  zoom_menu_item_model_ =
      std::make_unique<ui::ButtonMenuItemModel>(IDS_ZOOM_MENU, this);
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_MINUS,
                                                  IDS_ZOOM_MINUS2);
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_PLUS,
                                                  IDS_ZOOM_PLUS2);
  zoom_menu_item_model_->AddImageItem(IDC_FULLSCREEN);
  AddButtonItem(IDC_ZOOM_MENU, zoom_menu_item_model_.get());
}

bool AppMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the app menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing app menu. To fix this we need to some
  // how update the menu if new errors are added.
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())->errors();
  bool menu_items_added = false;
  for (auto* error : errors) {
    DCHECK(error);
    if (error->HasMenuItem()) {
      AddItem(error->MenuItemCommandID(), error->MenuItemLabel());
      SetIcon(GetIndexOfCommandId(error->MenuItemCommandID()).value(),
              error->MenuItemIcon());
      menu_items_added = true;
    }
  }
  return menu_items_added;
}

bool AppMenuModel::AddSafetyHubMenuItem() {
  // TODO(crbug.com/1443466): Remove when the service is only created when the
  // feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return false;
  }
  auto* const safety_hub_menu_notification_service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(
          browser_->profile());
  if (!safety_hub_menu_notification_service) {
    return false;
  }
  absl::optional<MenuNotificationEntry> notification =
      safety_hub_menu_notification_service->GetNotificationToShow();
  if (!notification.has_value()) {
    return false;
  }
  base::UmaHistogramEnumeration("Settings.SafetyHub.Impression",
                                safety_hub::SafetyHubSurfaces::kThreeDotMenu);
  base::UmaHistogramEnumeration(
      "Settings.SafetyHub.EntryPointImpression",
      safety_hub::SafetyHubEntryPoint::kMenuNotifications);
  absl::optional<safety_hub::SafetyHubModuleType> sh_module =
      safety_hub_menu_notification_service->GetModuleOfActiveNotification();
  if (sh_module.has_value()) {
    base::UmaHistogramEnumeration(
        "Settings.SafetyHub.MenuNotificationImpression", sh_module.value());
  }
  const auto safety_hub_icon = ui::ImageModel::FromVectorIcon(
      kSecurityIcon, ui::kColorMenuIcon, kDefaultIconSize);
  AddItemWithIcon(notification->command, notification->label, safety_hub_icon);
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void AppMenuModel::UpdateSettingsItemState() {
  bool is_disabled =
      policy::SystemFeaturesDisableListPolicyHandler::IsSystemFeatureDisabled(
          policy::SystemFeature::kBrowserSettings,
          g_browser_process->local_state());

  absl::optional<size_t> index = GetIndexOfCommandId(IDC_OPTIONS);
  if (index.has_value())
    SetEnabledAt(index.value(), !is_disabled);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(IDC_HELP_MENU);
  if (index.has_value()) {
    ui::SimpleMenuModel* help_menu =
        static_cast<ui::SimpleMenuModel*>(GetSubmenuModelAt(index.value()));
    index = help_menu->GetIndexOfCommandId(IDC_ABOUT);
    if (index.has_value())
      help_menu->SetEnabledAt(index.value(), !is_disabled);
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(IDC_ABOUT);
  if (index.has_value())
    SetEnabledAt(index.value(), !is_disabled);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_CHROMEOS)
