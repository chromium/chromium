// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

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
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
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
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/install_banner_config.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/common/web_app_id.h"
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
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "components/lens/lens_features.h"
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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kProfileMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kProfileOpenGuestItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kBookmarksMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kTabGroupsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kDownloadsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kHistoryMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kMoreToolsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kIncognitoMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel,
                                      kPasswordAndAutofillMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kPasswordManagerMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kShowLensOverlay);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kShowSearchCompanion);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kSaveAndShareMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kCastTitleItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kInstallAppItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kCreateShortcutItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel,
                                      kSetBrowserAsDefaultMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kPerformanceMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kChromeLabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kReadingModeMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kManageExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kVisitChromeWebStoreMenuItem);

namespace {

////////////////////////////////////////////////////////////////////////////////
// AddItemWithStringIdAndVectorIcon
void AddItemWithStringIdAndVectorIcon(ui::SimpleMenuModel* model,
                                      int command_id,
                                      int string_id,
                                      const gfx::VectorIcon& vector_icon) {
  return model->AddItemWithStringIdAndIcon(
      command_id, string_id,
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
}

////////////////////////////////////////////////////////////////////////////////
// AddSubMenuWithStringIdAndVectorIcon
void AddSubMenuWithStringIdAndVectorIcon(ui::SimpleMenuModel* model,
                                         int command_id,
                                         int string_id,
                                         ui::MenuModel* sub_menu,
                                         const gfx::VectorIcon& vector_icon) {
  return model->AddSubMenuWithStringIdAndIcon(
      command_id, string_id, sub_menu,
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
}

// Conditionally return the update app menu item title based on upgrade detector
// state.
std::u16string GetUpgradeDialogTitleText() {
  if (UpgradeDetector::GetInstance()->is_outdated_install() ||
      UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_MENU_ITEM);
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE_ALT);
#else
  return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE);
#endif
}

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available.
std::u16string GetInstallPWALabel(const Browser* browser) {
  // There may be no active web contents in tests.
  auto* const web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::u16string();
  }
  if (!web_app::CanCreateWebApp(browser)) {
    return std::u16string();
  }
  // Don't allow apps created from chrome-extension urls.
  if (web_contents->GetLastCommittedURL().SchemeIs("chrome-extension")) {
    return std::u16string();
  }

  // TODO(b/328077967): Support async nature of AppBannerManager pipeline runs
  // with the menu model instead of needing this workaround to verify if an
  // non-installable site is installed.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (app_id &&
      provider->registrar_unsafe().IsInstallState(
          *app_id, {web_app::proto::INSTALLED_WITH_OS_INTEGRATION}) &&
      provider->registrar_unsafe().GetAppUserDisplayMode(*app_id) !=
          web_app::mojom::UserDisplayMode::kBrowser) {
    return std::u16string();
  }

  std::u16string install_page_as_app_label =
      l10n_util::GetStringUTF16(IDS_INSTALL_DIY_TO_OS_LAUNCH_SURFACE);
  webapps::AppBannerManager* banner =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!banner) {
    // Showing `Install Page as App` allows the user to refetch the manifest and
    // go through the install flow without relying on the AppBannerManager to
    // finish working.
    if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
      return install_page_as_app_label;
    }
    return std::u16string();
  }

  std::optional<webapps::InstallBannerConfig> install_config =
      banner->GetCurrentBannerConfig();
  if (!install_config) {
    // In some edge cases where the `AppBannerManager` pipeline hasn't run yet,
    // the information populated to be used for determining installability and
    // other parameters is not available. In this case, allow users to try
    // installability by refetching the manifest.
    if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
      return install_page_as_app_label;
    }
    return std::u16string();
  }
  CHECK_EQ(install_config->mode, webapps::AppBannerMode::kWebApp);
  webapps::InstallableWebAppCheckResult installable =
      banner->GetInstallableWebAppCheckResult();

  switch (installable) {
    case webapps::InstallableWebAppCheckResult::kUnknown:
      // Loading of the menu model is synchronous, so there could be a condition
      // where the `AppBannerManager` has not yet finished the pipeline while
      // the menu item has been triggered. In such a case,
      // `banner->GetInstallableWebAppCheckResult()` returns the default value
      // of `kUnknown`.
      // Show `Install Page as App` for that use-case, since that allows the
      // user to trigger the install flow to verify all the data required for
      // installability. The correct dialog will be shown to the user depending
      // on whether the app turns out to be installable or not.
      if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
        return install_page_as_app_label;
      }
      return std::u16string();
    case webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      // Returning an empty string here allows the `launch page as app` field to
      // get populated in place of the `install` strings.
      return std::u16string();
    case webapps::InstallableWebAppCheckResult::kNo:
      if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
        return install_page_as_app_label;
      }
      return std::u16string();
    case webapps::InstallableWebAppCheckResult::kYes_ByUserRequest:
    case webapps::InstallableWebAppCheckResult::kYes_Promotable:
      std::u16string app_name = install_config->GetWebOrNativeAppName();
      if (app_name.empty()) {
        // Prefer showing `Install Page as App` here, as users can set the name
        // of the installed app on the DIY app dialog anyway.
        if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
          return install_page_as_app_label;
        } else {
          return std::u16string();
        }
      }
      return l10n_util::GetStringFUTF16(
          IDS_INSTALL_TO_OS_LAUNCH_SURFACE,
          ui::EscapeMenuLabelAmpersands(app_name));
  }
}

// TODO(b/328077967): Implement async updates of menu for app icon.
ui::ImageModel GetInstallPWAIcon(Browser* browser) {
  ui::ImageModel app_icon_to_use = ui::ImageModel::FromVectorIcon(
      kInstallDesktopChromeRefreshIcon, ui::kColorMenuIcon,
      ui::SimpleMenuModel::kDefaultIconSize);

  // App icons in the app menu are only a part of the WebAppUniversalInstall
  // feature.
  if (!base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
    return app_icon_to_use;
  }

  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return app_icon_to_use;
  }

  webapps::AppBannerManager* const banner =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!banner) {
    return app_icon_to_use;
  }

  // For sites that are not installable (DIY apps), do not return any icons,
  // instead use the default chrome refresh icon for installing.
  auto installable_check_result = banner->GetInstallableWebAppCheckResult();
  if (installable_check_result == webapps::InstallableWebAppCheckResult::kNo ||
      installable_check_result ==
          webapps::InstallableWebAppCheckResult::kUnknown) {
    return app_icon_to_use;
  }

  std::optional<webapps::WebAppBannerData> install_config =
      banner->GetCurrentWebAppBannerData();

  // If no data or no icons have been obtained by the AppBannerManager, return
  // the default icon.
  if (!install_config || install_config->primary_icon.empty()) {
    return app_icon_to_use;
  }

  gfx::ImageSkia primary_icon =
      gfx::ImageSkia::CreateFrom1xBitmap(install_config->primary_icon);
  gfx::ImageSkia resized_app_icon =
      gfx::ImageSkiaOperations::CreateResizedImage(
          primary_icon, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(ui::SimpleMenuModel::kDefaultIconSize,
                    ui::SimpleMenuModel::kDefaultIconSize));
  app_icon_to_use = ui::ImageModel::FromImageSkia(resized_app_icon);
  return app_icon_to_use;
}

// Returns the appropriate menu label for the IDC_OPEN_IN_PWA_WINDOW command if
// available.
std::u16string GetOpenPWALabel(const Browser* browser) {
  std::optional<webapps::AppId> app_id =
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

std::u16string GetSyncSectionTitle(Profile* profile,
                                   signin::IdentityManager* identity_manager) {
  const AccountInfo account = GetAccountInfoFromProfile(profile);

  if (IsSyncPaused(profile) || account.IsEmpty()) {
    return l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
  }

  if (signin_util::IsSigninPending(identity_manager)) {
    return base::UTF8ToUTF16(account.email);
  }

  return l10n_util::GetStringFUTF16(
      IDS_PROFILE_ROW_SIGNED_IN_MESSAGE_WITH_EMAIL,
      {base::UTF8ToUTF16(account.email)});
}

class ProfileSubMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  ProfileSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                      Profile* profile,
                      const ui::ColorProvider* color_provider);
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
    Profile* profile,
    const ui::ColorProvider* color_provider)
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
              ? profile_attributes->GetAvatarIcon(
                    avatar_icon_size, /*use_high_res_file=*/true,
                    GetPlaceholderAvatarIconParamsDependingOnTheme(
                        ThemeServiceFactory::GetForProfile(profile),
                        /*background_color_id=*/ui::kColorMenuBackground,
                        *color_provider))
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
    AddItemWithStringIdAndVectorIcon(this, IDC_CUSTOMIZE_CHROME,
                                     IDS_CUSTOMIZE_CHROME,
                                     vector_icons::kEditChromeRefreshIcon);
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
    AddItemWithStringIdAndVectorIcon(this, IDC_MANAGE_GOOGLE_ACCOUNT,
                                     IDS_MANAGE_GOOGLE_ACCOUNT,
                                     manage_account_icon);
  }

  if (!profile->IsIncognitoProfile() && !profile->IsGuestSession()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddTitle(l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
    auto profile_entries = GetAllOtherProfileEntriesForProfileSubMenu(profile);
    profiles::PlaceholderAvatarIconParams icon_params =
        GetPlaceholderAvatarIconParamsVisibleAgainstColor(
            color_provider->GetColor(ui::kColorMenuBackground));
    for (ProfileAttributesEntry* profile_entry : profile_entries) {
      std::u16string display_name = GetProfileMenuDisplayName(profile_entry);
      int menu_id = GetAndIncrementNextMenuID();
      AddItemWithIcon(
          menu_id,
          ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
              display_name,
              GetLayoutConstant(APP_MENU_MAXIMUM_CHARACTER_LENGTH),
              gfx::CHARACTER_BREAK)),
          ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
              profile_entry->GetAvatarIcon(
                  avatar_icon_size, /*use_high_res_file=*/true, icon_params),
              avatar_icon_size, avatar_icon_size, profiles::SHAPE_CIRCLE)));
      other_profiles_.insert({menu_id, profile_entry->GetPath()});
    }

    if (profiles::IsGuestModeEnabled(*profile)) {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_OPEN_GUEST_PROFILE, IDS_OPEN_GUEST_PROFILE,
          vector_icons::kAccountCircleChromeRefreshIcon);
      SetElementIdentifierAt(
          GetIndexOfCommandId(IDC_OPEN_GUEST_PROFILE).value(),
          AppMenuModel::kProfileOpenGuestItem);
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    if (profiles::IsProfileCreationAllowed()) {
      AddItemWithStringIdAndVectorIcon(this, IDC_ADD_NEW_PROFILE,
                                       IDS_ADD_NEW_PROFILE,
                                       kAccountAddChromeRefreshIcon);
    }
    AddItemWithStringIdAndVectorIcon(this, IDC_MANAGE_CHROME_PROFILES,
                                     IDS_MANAGE_CHROME_PROFILES,
                                     kAccountManageChromeRefreshIcon);
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

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  AddTitle(GetSyncSectionTitle(profile_, identity_manager));

  const bool is_sync_feature_enabled =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  // First, check for sync errors. They may exist even if sync-the-feature is
  // disabled and only sync-the-transport is running.
  const std::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile_);
  if (error.has_value()) {
    if (error == AvatarSyncErrorType::kSyncPaused) {
      // If sync is paused the menu item will be specific to the paused error.
      AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_SIGNIN_WHEN_PAUSED,
                                       IDS_PROFILE_ROW_SIGN_IN_AGAIN,
                                       vector_icons::kSyncOffChromeRefreshIcon);
    } else {
      // All remaining errors will have the same menu item.
      AddItemWithStringIdAndVectorIcon(
          this, IDC_SHOW_SYNC_SETTINGS, IDS_PROFILE_ROW_SYNC_ERROR_MESSAGE,
          vector_icons::kSyncProblemChromeRefreshIcon);
    }
    return true;
  }

  if (signin_util::IsSigninPending(identity_manager)) {
    AddItemWithStringIdAndVectorIcon(
        this, IDC_SHOW_SIGNIN_WHEN_PAUSED, IDS_PROFILES_VERIFY_ACCOUNT_BUTTON,
        vector_icons::kAccountCircleOffChromeRefreshIcon);
  } else if (is_sync_feature_enabled) {
    AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_SYNC_SETTINGS,
                                     IDS_PROFILE_ROW_SYNC_IS_ON,
                                     vector_icons::kSyncChromeRefreshIcon);
  } else {
    AddItemWithStringIdAndVectorIcon(this, IDC_TURN_ON_SYNC,
                                     IDS_PROFILE_ROW_TURN_ON_SYNC,
                                     vector_icons::kSyncOffChromeRefreshIcon);
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
  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_PASSWORD_MANAGER,
                                   IDS_VIEW_PASSWORDS,
                                   vector_icons::kPasswordManagerIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_PASSWORD_MANAGER).value(),
                         AppMenuModel::kPasswordManagerMenuItem);
  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_PAYMENT_METHODS,
                                   IDS_PAYMENT_METHOD_SUBMENU_OPTION,
                                   kCreditCardChromeRefreshIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_ADDRESSES,
                                   IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
                                   vector_icons::kLocationOnChromeRefreshIcon);
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
  AddItemWithStringIdAndVectorIcon(this, IDC_FIND, IDS_FIND, kSearchMenuIcon);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndVectorIcon(this, IDC_CUT, IDS_CUT, kCutMenuIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_COPY, IDS_COPY, kCopyMenuIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_PASTE, IDS_PASTE, kPasteMenuIcon);
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
  if (media_router::MediaRouterEnabled(browser->profile())) {
    AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_CAST));
    SetElementIdentifierAt(GetItemCount() - 1, AppMenuModel::kCastTitleItem);
    AddItemWithStringIdAndVectorIcon(this, IDC_ROUTE_MEDIA,
                                     IDS_MEDIA_ROUTER_MENU_ITEM_TITLE,
                                     kCastChromeRefreshIcon);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SAVE));
  AddItemWithStringIdAndVectorIcon(this, IDC_SAVE_PAGE, IDS_SAVE_PAGE,
                                   kFileSaveChromeRefreshIcon);
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (std::u16string install_item = GetInstallPWALabel(browser);
      !install_item.empty()) {
    AddItemWithIcon(IDC_INSTALL_PWA, install_item, GetInstallPWAIcon(browser));
    SetElementIdentifierAt(GetItemCount() - 1, AppMenuModel::kInstallAppItem);
  } else if (std::u16string open_item = GetOpenPWALabel(browser);
             !open_item.empty()) {
    AddItemWithIcon(
        IDC_OPEN_IN_PWA_WINDOW, open_item,
        ui::ImageModel::FromVectorIcon(kDesktopWindowsChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
  }
  AddItemWithStringIdAndVectorIcon(this, IDC_CREATE_SHORTCUT,
                                   IDS_ADD_TO_OS_LAUNCH_SURFACE,
                                   kDriveShortcutChromeRefreshIcon);
  SetElementIdentifierAt(GetItemCount() - 1, AppMenuModel::kCreateShortcutItem);
  if (!sharing_hub::SharingIsDisabledByPolicy(browser->profile()) ||
      media_router::MediaRouterEnabled(browser->profile())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SHARE));
    if (!sharing_hub::SharingIsDisabledByPolicy(browser->profile())) {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_COPY_URL, IDS_APP_MENU_COPY_LINK, kLinkChromeRefreshIcon);
      AddItemWithStringIdAndVectorIcon(this, IDC_SEND_TAB_TO_SELF,
                                       IDS_MENU_SEND_TAB_TO_SELF,
                                       kDevicesChromeRefreshIcon);
      AddItemWithStringIdAndVectorIcon(this, IDC_QRCODE_GENERATOR,
                                       IDS_APP_MENU_CREATE_QR_CODE,
                                       kQrCodeChromeRefreshIcon);
    }
  }
  if (sharing_hub::DesktopScreenshotsFeatureEnabled(browser->profile())) {
    AddItemWithStringIdAndVectorIcon(this, IDC_SHARING_HUB_SCREENSHOT,
                                     IDS_SHARING_HUB_SCREENSHOT_LABEL,
                                     kSharingHubScreenshotIcon);
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
    AddItemWithStringIdAndVectorIcon(this, IDC_ABOUT, IDS_ABOUT,
                                     vector_icons::kInfoRefreshIcon);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (whats_new::IsEnabled()) {
      AddItemWithStringIdAndVectorIcon(this, IDC_CHROME_WHATS_NEW,
                                       IDS_CHROME_WHATS_NEW, kReleaseAlertIcon);
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    AddItemWithStringId(IDC_HELP_PAGE_VIA_MENU, help_string_id);
    if (browser_defaults::kShowHelpMenuItemIcon) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE_VIA_MENU).value(),
              ui::ImageModel::FromImage(rb.GetNativeImageNamed(IDR_HELP_MENU)));
    } else {
      SetCommandIcon(this, IDC_HELP_PAGE_VIA_MENU, kHelpMenuIcon);
    }
    if (browser->profile()->GetPrefs()->GetBoolean(
            prefs::kUserFeedbackAllowed)) {
      AddItemWithStringIdAndVectorIcon(this, IDC_FEEDBACK, IDS_FEEDBACK,
                                       kReportIcon);
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
  if (base::FeatureList::IsEnabled(features::kTabOrganizationAppMenuItem) &&
      TabOrganizationUtils::GetInstance()->IsEnabled(browser->profile())) {
    auto* const tab_organization_service =
        TabOrganizationServiceFactory::GetForProfile(browser->profile());
    if (tab_organization_service) {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_ORGANIZE_TABS, IDS_TAB_ORGANIZE_MENU, kAutoTabGroupsIcon);
      SetIsNewFeatureAt(
          GetIndexOfCommandId(IDC_ORGANIZE_TABS).value(),
          browser->window()->MaybeShowNewBadgeFor(features::kTabOrganization));
    }
  }

  if (base::FeatureList::IsEnabled(features::kTabstripDeclutter)) {
    AddItemWithStringIdAndVectorIcon(this, IDC_DECLUTTER_TABS,
                                     IDS_DECLUTTER_MENU, kTabCloseInactiveIcon);
    SetIsNewFeatureAt(
        GetIndexOfCommandId(IDC_DECLUTTER_TABS).value(),
        browser->window()->MaybeShowNewBadgeFor(features::kTabstripDeclutter));
  }

  AddItemWithStringIdAndVectorIcon(this, IDC_NAME_WINDOW, IDS_NAME_WINDOW,
                                   kNameWindowIcon);

  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_READING_MODE_SIDE_PANEL,
                                   IDS_SHOW_READING_MODE_SIDE_PANEL,
                                   kMenuBookChromeRefreshIcon);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_SHOW_READING_MODE_SIDE_PANEL).value(),
      kReadingModeMenuItem);

  if (base::FeatureList::IsEnabled(features::kToolbarPinning) &&
      CustomizeChromePageHandler::IsSupported(
          NtpCustomBackgroundServiceFactory::GetForProfile(browser->profile()),
          browser->profile())) {
    AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
                                     IDS_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
                                     kEditChromeRefreshIcon);
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  if (!features::IsExtensionMenuInRootAppMenu()) {
    AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);
  }

  AddItemWithStringIdAndVectorIcon(this, IDC_PERFORMANCE, IDS_SHOW_PERFORMANCE,
                                   kPerformanceIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_PERFORMANCE).value(),
                         kPerformanceMenuItem);

  if (chrome::CanOpenTaskManager()) {
    AddItemWithStringIdAndVectorIcon(this, IDC_TASK_MANAGER, IDS_TASK_MANAGER,
                                     kTaskManagerIcon);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddItemWithStringId(IDC_TAKE_SCREENSHOT, IDS_TAKE_SCREENSHOT);
#endif
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndVectorIcon(this, IDC_DEV_TOOLS, IDS_DEV_TOOLS,
                                   kDeveloperToolsIcon);

  if (base::debug::IsProfilingSupported()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
  }
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
        AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_CHROME_LABS,
                                         IDS_CHROMELABS, kScienceIcon);
        SetElementIdentifierAt(
            GetIndexOfCommandId(IDC_SHOW_CHROME_LABS).value(),
            kChromeLabsMenuItem);
      }
    }
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
  AddItemWithStringIdAndVectorIcon(
      this, IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
      IDS_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS_ITEM,
      vector_icons::kExtensionChromeRefreshIcon);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS).value(),
      kManageExtensionsMenuItem);
  AddItemWithStringId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                      IDS_EXTENSIONS_SUBMENU_CHROME_WEBSTORE_ITEM);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE)
          .value(),
      kVisitChromeWebStoreMenuItem);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  SetCommandIcon(this, IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                 vector_icons::kGoogleChromeWebstoreIcon);
#endif
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
    safety_hub::SafetyHubModuleType sh_module,
    int event_flags) {
  base::UmaHistogramEnumeration("Settings.SafetyHub.Interaction",
                                safety_hub::SafetyHubSurfaces::kThreeDotMenu);
  base::UmaHistogramEnumeration(
      "Settings.SafetyHub.EntryPointInteraction",
      safety_hub::SafetyHubEntryPoint::kMenuNotifications);
  base::UmaHistogramEnumeration("Settings.SafetyHub.MenuNotificationClicked",
                                sh_module);
  if (SafetyHubHatsService* hats_service =
          SafetyHubHatsServiceFactory::GetForProfile(browser_->profile())) {
    hats_service->SafetyHubNotificationClicked();
  }
}

void AppMenuModel::LogMenuMetrics(int command_id) {
  base::TimeDelta delta = timer_.Elapsed();

  switch (command_id) {
    case IDC_UPGRADE_DIALOG:
      LogMenuAction(MENU_ACTION_UPGRADE_DIALOG);
      break;
    case IDC_NEW_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewTab", delta);
      }
      LogMenuAction(MENU_ACTION_NEW_TAB);
      break;
    case IDC_NEW_WINDOW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewWindow",
                                      delta);
      }
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
      browser()->window()->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHPowerBookmarksSidePanelFeature,
          FeaturePromoFeatureUsedAction::kIgnorePromoIfPresent);
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
    // Lens overlay.
    case IDC_CONTENT_CONTEXT_LENS_OVERLAY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowLensOverlay",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_LENS_OVERLAY);
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.RestoreTab",
                                      delta);
      }
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
    case IDC_FIND:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Find", delta);
      }
      LogMenuAction(MENU_ACTION_FIND);
      break;
    case IDC_PRINT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Print", delta);
      }
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Cut", delta);
      }
      LogMenuAction(MENU_ACTION_CUT);
      break;
    case IDC_COPY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Copy", delta);
      }
      LogMenuAction(MENU_ACTION_COPY);
      break;
    case IDC_PASTE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Paste", delta);
      }
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ViewSource",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_VIEW_SOURCE);
      break;
    case IDC_DEV_TOOLS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevTools",
                                      delta);
      }
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
      browser()->window()->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHReadingModeSidePanelFeature,
          FeaturePromoFeatureUsedAction::kIgnorePromoIfPresent);
      break;
    case IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowCustomizeChromeSidePanel", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL);
      // Close IPH for side panel menu, if shown.
      browser()->window()->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature,
          FeaturePromoFeatureUsedAction::kIgnorePromoIfPresent);
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Settings",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPTIONS);
      base::UmaHistogramEnumeration(
          "Settings.OpenSettingsFromMenu.PerProfileType",
          profile_metrics::GetBrowserProfileType(browser_->profile()));
      break;
    case IDC_ABOUT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.About", delta);
      }
      LogMenuAction(MENU_ACTION_ABOUT);
      break;
    // Help menu.
    case IDC_HELP_PAGE_VIA_MENU:
      base::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.HelpPage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_HELP_PAGE_VIA_MENU);
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_SHOW_BETA_FORUM:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BetaForum",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BETA_FORUM);
      break;
    case IDC_FEEDBACK:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Feedback",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_FEEDBACK);
      break;
    case IDC_CHROME_TIPS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ChromeTips",
                                      delta);
      }
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Exit", delta);
      }
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
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.AppInfo", delta);
      }
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
    case IDC_SET_BROWSER_AS_DEFAULT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SetBrowserAsDefault", delta);
      }
      LogMenuAction(MENU_ACTION_SET_BROWSER_AS_DEFAULT);
      break;
    case IDC_SAFETY_HUB_SHOW_PASSWORD_CHECKUP:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SafetyHubNotificationPasswordCheck",
            delta);
      }
      LogMenuAction(MENU_ACTION_SAFETY_HUB_SHOW_PASSWORD_CHECKUP);
      break;
    case IDC_OPEN_SAFETY_HUB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SafetyHubNotificationOpenSafetyHub",
            delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_SAFETY_HUB);
      break;
    case IDC_SAFETY_HUB_MANAGE_EXTENSIONS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SafetyHubNotificationManageExtensions",
            delta);
      }
      LogMenuAction(MENU_ACTION_SAFETY_HUB_MANAGE_EXTENSIONS);
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
  if (command_id == IDC_PROFILING_ENABLED) {
    return content::Profiling::BeingProfiled();
  }
  if (command_id == IDC_TOGGLE_REQUEST_TABLET_SITE) {
    return chrome::IsRequestingTabletSite(browser_);
  }

  return false;
}

bool AppMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    return true;
  }

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

  if (app_menu_icon_controller_ &&
      app_menu_icon_controller_->GetTypeAndSeverity().type ==
          AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    AddSeparator(ui::SPACING_SEPARATOR);
    const auto update_icon = ui::ImageModel::FromVectorIcon(
        kBrowserToolsUpdateChromeRefreshIcon,
        ui::kColorMenuIconOnEmphasizedBackground, kDefaultIconSize);
    if (browser_defaults::kShowUpgradeMenuItem) {
      AddItemWithIcon(IDC_UPGRADE_DIALOG, GetUpgradeDialogTitleText(),
                      update_icon);
      AddSeparator(ui::SPACING_SEPARATOR);
    }
  }

  if (AddSafetyHubMenuItem() || AddGlobalErrorMenuItems() ||
      AddDefaultBrowserMenuItems()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  AddItemWithStringIdAndVectorIcon(
      this, IDC_NEW_TAB,
      browser_->profile()->IsIncognitoProfile() &&
              !browser_->profile()->IsGuestSession()
          ? IDS_NEW_INCOGNITO_TAB
          : IDS_NEW_TAB,
      kNewTabRefreshIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_NEW_WINDOW, IDS_NEW_WINDOW,
                                   kNewWindowIcon);

  // This menu item is not visible in Guest Mode. If incognito mode is not
  // available, it will be shown in disabled state. (crbug.com/1100791)
  if (!browser_->profile()->IsGuestSession()) {
    AddItemWithStringIdAndVectorIcon(this, IDC_NEW_INCOGNITO_WINDOW,
                                     IDS_NEW_INCOGNITO_WINDOW,
                                     kIncognitoRefreshMenuIcon);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_NEW_INCOGNITO_WINDOW).value(),
        kIncognitoMenuItem);
  }

  AddSeparator(ui::NORMAL_SEPARATOR);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  sub_menus_.push_back(std::make_unique<ProfileSubMenuModel>(
      this, browser()->profile(), browser()->window()->GetColorProvider()));
  auto* const profile_submenu_model =
      static_cast<ProfileSubMenuModel*>(sub_menus_.back().get());
  AddSubMenu(IDC_PROFILE_MENU_IN_APP_MENU,
             profile_submenu_model->profile_name(), profile_submenu_model);
  SetIcon(GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value(),
          profile_submenu_model->avatar_image_model());
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_PROFILE_MENU_IN_APP_MENU).value(),
      kProfileMenuItem);
  AddSeparator(ui::SPACING_SEPARATOR);
#endif

  if (!browser_->profile()->IsGuestSession()) {
    sub_menus_.push_back(
        std::make_unique<PasswordsAndAutofillSubMenuModel>(this));
    AddSubMenuWithStringIdAndVectorIcon(
        this, IDC_PASSWORDS_AND_AUTOFILL_MENU, IDS_PASSWORDS_AND_AUTOFILL_MENU,
        sub_menus_.back().get(), vector_icons::kPasswordManagerIcon);
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
    AddSubMenuWithStringIdAndVectorIcon(this, IDC_RECENT_TABS_MENU,
                                        IDS_HISTORY_MENU,
                                        sub_menus_.back().get(), kHistoryIcon);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_RECENT_TABS_MENU).value(),
                           kHistoryMenuItem);
  }
  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS,
                                   kDownloadMenuIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_DOWNLOADS).value(),
                         kDownloadsMenuItem);
  if (!browser_->profile()->IsGuestSession()) {
    bookmark_sub_menu_model_ =
        std::make_unique<BookmarkSubMenuModel>(this, browser_);

    AddSubMenuWithStringIdAndVectorIcon(
        this, IDC_BOOKMARKS_MENU, IDS_BOOKMARKS_AND_LISTS_MENU,
        bookmark_sub_menu_model_.get(), kBookmarksListsMenuIcon);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_BOOKMARKS_MENU).value(),
                           kBookmarksMenuItem);
  }

  if (tab_groups::IsTabGroupsSaveUIUpdateEnabled() &&
      browser_->profile()->IsRegularProfile()) {
    auto saved_tab_groups_model = std::make_unique<ui::SimpleMenuModel>(this);
    sub_menus_.push_back(std::move(saved_tab_groups_model));
    AddSubMenuWithStringIdAndVectorIcon(
        this, IDC_SAVED_TAB_GROUPS_MENU, IDS_SAVED_TAB_GROUPS_MENU,
        sub_menus_.back().get(), kSavedTabGroupBarEverythingIcon);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_SAVED_TAB_GROUPS_MENU).value(),
        kTabGroupsMenuItem);
  }

  if (features::IsExtensionMenuInRootAppMenu()) {
    // Extensions sub menu.
    sub_menus_.push_back(std::make_unique<ExtensionsMenuModel>(this, browser_));
    AddSubMenuWithStringIdAndVectorIcon(
        this, IDC_EXTENSIONS_SUBMENU, IDS_EXTENSIONS_SUBMENU,
        sub_menus_.back().get(), vector_icons::kExtensionChromeRefreshIcon);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU).value(),
                           kExtensionsMenuItem);
  }
  AddItemWithStringIdAndVectorIcon(this, IDC_CLEAR_BROWSING_DATA,
                                   IDS_CLEAR_BROWSING_DATA,
                                   kTrashCanRefreshIcon);

  AddSeparator(ui::NORMAL_SEPARATOR);
  CreateZoomMenu();
  AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringIdAndVectorIcon(this, IDC_PRINT, IDS_PRINT, kPrintMenuIcon);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (browser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled()) {
    AddItemWithStringIdAndVectorIcon(
        this, IDC_CONTENT_CONTEXT_LENS_OVERLAY, IDS_SHOW_LENS_OVERLAY,
        vector_icons::kGoogleLensMonochromeLogoIcon);
    const int lens_command_index =
        GetIndexOfCommandId(IDC_CONTENT_CONTEXT_LENS_OVERLAY).value();
    SetElementIdentifierAt(lens_command_index, kShowLensOverlay);
    SetIsNewFeatureAt(lens_command_index,
                      browser()->window()->MaybeShowNewBadgeFor(
                          lens::features::kLensOverlay));
  } else if (companion::IsSearchInCompanionSidePanelSupported(browser())) {
    AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_SEARCH_COMPANION,
                                     IDS_SHOW_SEARCH_COMPANION,
                                     vector_icons::kGoogleGLogoMonochromeIcon);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_SHOW_SEARCH_COMPANION).value(),
        kShowSearchCompanion);
  }
#endif

  AddItemWithStringIdAndVectorIcon(this, IDC_SHOW_TRANSLATE, IDS_SHOW_TRANSLATE,
                                   kTranslateIcon);

  CreateFindAndEditSubMenu();

  sub_menus_.push_back(
      std::make_unique<SaveAndShareSubMenuModel>(this, browser_));
  int string_id = media_router::MediaRouterEnabled(browser()->profile())
                      ? IDS_CAST_SAVE_AND_SHARE_MENU
                      : IDS_SAVE_AND_SHARE_MENU;
  AddSubMenuWithStringIdAndVectorIcon(this, IDC_SAVE_AND_SHARE_MENU, string_id,
                                      sub_menus_.back().get(),
                                      kFileSaveChromeRefreshIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SAVE_AND_SHARE_MENU).value(),
                         kSaveAndShareMenuItem);

#if BUILDFLAG(IS_CHROMEOS)
  // Always show this option if we're in tablet mode on Chrome OS.
  if (display::Screen::GetScreen()->InTabletMode()) {
    AddItemWithStringIdAndVectorIcon(this, IDC_TOGGLE_REQUEST_TABLET_SITE,
                                     IDS_TOGGLE_REQUEST_TABLET_SITE,
                                     chrome::IsRequestingTabletSite(browser_)
                                         ? kRequestMobileSiteCheckedIcon
                                         : kRequestMobileSiteUncheckedIcon);
  }
#endif

  sub_menus_.push_back(std::make_unique<ToolsMenuModel>(this, browser_));
  AddSubMenuWithStringIdAndVectorIcon(
      this, IDC_MORE_TOOLS_MENU, IDS_MORE_TOOLS_MENU, sub_menus_.back().get(),
      kMoreToolsMenuIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_MORE_TOOLS_MENU).value(),
                         kMoreToolsMenuItem);

  AddSeparator(ui::NORMAL_SEPARATOR);
// The help submenu is only displayed on official Chrome builds. As the
// 'About' item has been moved to this submenu, it's reinstated here for
// Chromium builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  sub_menus_.push_back(std::make_unique<HelpMenuModel>(this, browser_));
  AddSubMenuWithStringIdAndVectorIcon(this, IDC_HELP_MENU, IDS_HELP_MENU,
                                      sub_menus_.back().get(), kHelpMenuIcon);
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#else
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#endif
#endif

  AddItemWithStringIdAndVectorIcon(this, IDC_OPTIONS, IDS_SETTINGS,
                                   kSettingsMenuIcon);

  if (browser_defaults::kShowExitMenuItem) {
    AddItemWithStringIdAndVectorIcon(this, IDC_EXIT, IDS_EXIT, kExitMenuIcon);
  }

  // On Chrome OS, similar UI is displayed in the system tray menu, instead of
  // this menu.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (chrome::ShouldDisplayManagedUi(browser_->profile())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(IDC_SHOW_MANAGEMENT_PAGE,
                    chrome::GetManagedUiMenuItemLabel(browser_->profile()),
                    ui::ImageModel::FromVectorIcon(
                        chrome::GetManagedUiIcon(browser_->profile()),
                        ui::kColorMenuIcon, kDefaultIconSize));

    SetAccessibleNameAt(
        GetIndexOfCommandId(IDC_SHOW_MANAGEMENT_PAGE).value(),
        chrome::GetManagedUiMenuItemTooltip(browser_->profile()));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
  AddSubMenuWithStringIdAndVectorIcon(this, IDC_FIND_AND_EDIT_MENU,
                                      IDS_FIND_AND_EDIT_MENU,
                                      sub_menus_.back().get(), kSearchMenuIcon);
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
  SetCommandIcon(this, IDC_ZOOM_MENU, kZoomInIcon);
}

bool AppMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the app menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing app menu. To fix this we need to some
  // how update the menu if new errors are added.
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())->errors();
  bool menu_items_added = false;
  for (GlobalError* error : errors) {
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

bool AppMenuModel::AddDefaultBrowserMenuItems() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  if (browser_->profile()->IsIncognitoProfile() ||
      browser_->profile()->IsGuestSession()) {
    return false;
  }

  if ((app_menu_icon_controller_ &&
       app_menu_icon_controller_->GetTypeAndSeverity().type ==
           AppMenuIconController::IconType::DEFAULT_BROWSER_PROMPT) ||
      (DefaultBrowserPromptManager::GetInstance()->get_show_app_menu_item())) {
    AddItemWithIcon(
        IDC_SET_BROWSER_AS_DEFAULT,
        l10n_util::GetStringUTF16(IDS_SET_BROWSER_AS_DEFAULT_MENU_ITEM),
        ui::ImageModel::FromVectorIcon(omnibox::kProductChromeRefreshIcon,
                                       ui::kColorMenuIcon, kDefaultIconSize));
    SetElementIdentifierAt(GetItemCount() - 1,
                           AppMenuModel::kSetBrowserAsDefaultMenuItem);
    return true;
  }
#endif
  return false;
}

bool AppMenuModel::AddSafetyHubMenuItem() {
  // TODO(crbug.com/40267370): Remove when the service is only created when the
  // feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return false;
  }
  auto* safety_hub_menu_notification_service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(
          browser_->profile());
  if (!safety_hub_menu_notification_service) {
    return false;
  }
  std::optional<MenuNotificationEntry> notification =
      safety_hub_menu_notification_service->GetNotificationToShow();
  if (!notification.has_value()) {
    return false;
  }
  base::UmaHistogramEnumeration("Settings.SafetyHub.Impression",
                                safety_hub::SafetyHubSurfaces::kThreeDotMenu);
  base::UmaHistogramEnumeration(
      "Settings.SafetyHub.EntryPointImpression",
      safety_hub::SafetyHubEntryPoint::kMenuNotifications);
  base::UmaHistogramEnumeration("Settings.SafetyHub.MenuNotificationImpression",
                                notification->module);
  const auto safety_hub_icon = ui::ImageModel::FromVectorIcon(
      kSecurityIcon, ui::kColorMenuIcon, kDefaultIconSize);
  AddItemWithIcon(notification->command, notification->label, safety_hub_icon);
  SetExecuteCallbackAt(
      GetIndexOfCommandId(notification->command).value(),
      base::BindRepeating(&AppMenuModel::LogSafetyHubInteractionMetrics,
                          base::Unretained(this), notification->module));
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void AppMenuModel::UpdateSettingsItemState() {
  bool is_disabled =
      policy::SystemFeaturesDisableListPolicyHandler::IsSystemFeatureDisabled(
          policy::SystemFeature::kBrowserSettings,
          g_browser_process->local_state());

  std::optional<size_t> index = GetIndexOfCommandId(IDC_OPTIONS);
  if (index.has_value()) {
    SetEnabledAt(index.value(), !is_disabled);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(IDC_HELP_MENU);
  if (index.has_value()) {
    ui::SimpleMenuModel* help_menu =
        static_cast<ui::SimpleMenuModel*>(GetSubmenuModelAt(index.value()));
    index = help_menu->GetIndexOfCommandId(IDC_ABOUT);
    if (index.has_value()) {
      help_menu->SetEnabledAt(index.value(), !is_disabled);
    }
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(IDC_ABOUT);
  if (index.has_value()) {
    SetEnabledAt(index.value(), !is_disabled);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_CHROMEOS)
