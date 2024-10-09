// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace {

// Helpers --------------------------------------------------------------------

std::u16string GetSyncErrorButtonText(AvatarSyncErrorType error) {
  switch (error) {
    case AvatarSyncErrorType::kSyncPaused:
    case AvatarSyncErrorType::kUnrecoverableError:
      // The user was signed out. Offer them to sign in again.
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON);
    case AvatarSyncErrorType::kManagedUserUnrecoverableError:
      // As opposed to the corresponding error in an unmanaged account
      // (AvatarSyncErrorType::kUnrecoverableError), sign-out hasn't happened
      // here yet. The button directs to the sign-out confirmation dialog in
      // settings.
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON);
    case AvatarSyncErrorType::kUpgradeClientError:
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON);
    case AvatarSyncErrorType::kPassphraseError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON);
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError:
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_BUTTON);
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForEverythingError:
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForPasswordsError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_RECOVERABILITY_BUTTON);
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
  }
}

std::u16string GetProfileIdentifier(
    std::u16string local_profile_name,
    const std::string& primary_account_info_name) {
  if (primary_account_info_name.empty()) {
    return local_profile_name;
  }

  return l10n_util::GetStringFUTF16(
      IDS_PROFILE_MENU_PROFILE_IDENTIFIER_WITH_SEPARATOR,
      base::UTF8ToUTF16(primary_account_info_name),
      std::move(local_profile_name));
}

}  // namespace

// ProfileMenuView ---------------------------------------------------------

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(views::Button* anchor_button, Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  set_close_on_deactivate(close_on_deactivate_for_testing_);
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::BuildMenu() {
  Profile* profile = browser()->profile();
  if (profile->IsGuestSession()) {
    BuildGuestIdentity();
  } else {
    CHECK(!profile->IsOffTheRecord());

    if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
      BuildIdentityWithCallToAction();
    } else {
      BuildIdentity();

      // Users should not be able to open chrome settings from WebApps.
      if (!web_app::AppBrowserController::IsWebApp(browser())) {
        BuildSyncInfo();
        BuildAutofillButtons();
      }
    }
  }

  // Users should not be able to use features from WebApps.
  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    BuildFeatureButtons();
  }

  if (!(profile->IsGuestSession())) {
    SetProfileManagementHeading(l10n_util::GetStringUTF16(
        switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
            ? IDS_PROFILE_MENU_PROFILES_LIST_TITLE
            : IDS_PROFILES_LIST_PROFILES_TITLE));
    BuildAvailableProfiles();

    // Users should not be able to manage profiles from WebApps.
    if (!web_app::AppBrowserController::IsWebApp(browser())) {
      BuildProfileManagementFeatureButtons();
    }
  }

  if (web_app::AppBrowserController::IsWebApp(browser())) {
    browser()->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

gfx::ImageSkia ProfileMenuView::GetSyncIcon() const {
  Profile* profile = browser()->profile();
  if (profile->IsOffTheRecord() || profile->IsGuestSession())
    return gfx::ImageSkia();

  bool is_sync_feature_enabled =
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSync);
  if (!is_sync_feature_enabled) {
    // This is done regardless of GetAvatarSyncErrorType() because the icon
    // should reflect that sync-the-feature is off. The error will still be
    // highlighted by other parts of the UI.
    return ColoredImageForMenu(kSyncDisabledChromeRefreshIcon,
                               kColorProfileMenuSyncOffIcon);
  }

  std::optional<AvatarSyncErrorType> error = GetAvatarSyncErrorType(profile);
  if (!error) {
    return ColoredImageForMenu(kSyncChromeRefreshIcon,
                               kColorProfileMenuSyncIcon);
  }

  ui::ColorId color_id = error == AvatarSyncErrorType::kSyncPaused
                                       ? kColorProfileMenuSyncPausedIcon
                                       : kColorProfileMenuSyncErrorIcon;
  return ColoredImageForMenu(kSyncDisabledChromeRefreshIcon, color_id);
}

std::u16string ProfileMenuView::GetAccessibleWindowTitle() const {
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);

  if (!menu_title_.empty()) {
    title = l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA, title,
                                       menu_title_);
  }
  if (!menu_subtitle_.empty()) {
    title = l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA, title,
                                       menu_subtitle_);
  }
  return title;
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  RecordClick(ActionableItem::kManageGoogleAccountButton);
  if (!perform_menu_actions())
    return;

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  NavigateToGoogleAccountPage(
      profile,
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

void ProfileMenuView::OnPasswordsButtonClicked() {
  RecordClick(ActionableItem::kPasswordsButton);
  if (!perform_menu_actions())
    return;
  NavigateToManagePasswordsPage(
      browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
}

void ProfileMenuView::OnCreditCardsButtonClicked() {
  RecordClick(ActionableItem::kCreditCardsButton);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
}

void ProfileMenuView::OnAddressesButtonClicked() {
  RecordClick(ActionableItem::kAddressesButton);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
}

void ProfileMenuView::OnGuestProfileButtonClicked() {
  RecordClick(ActionableItem::kGuestProfileButton);
  if (!perform_menu_actions())
    return;
  DCHECK(profiles::IsGuestModeEnabled(*browser()->profile()));
  profiles::SwitchToGuestProfile();
}

void ProfileMenuView::OnExitProfileButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  if (!perform_menu_actions())
    return;
  profiles::CloseProfileWindows(browser()->profile());
}

void ProfileMenuView::OnSyncSettingsButtonClicked() {
  RecordClick(ActionableItem::kSyncSettingsButton);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
}

void ProfileMenuView::OnSyncErrorButtonClicked(AvatarSyncErrorType error) {
  RecordClick(ActionableItem::kSyncErrorButton);
  if (!perform_menu_actions())
    return;

  // The logic below must be consistent with GetSyncInfoForAvatarErrorType().
  switch (error) {
    case AvatarSyncErrorType::kManagedUserUnrecoverableError:
      chrome::ShowSettingsSubPage(browser(), chrome::kSignOutSubPage);
      break;
    case AvatarSyncErrorType::kUnrecoverableError: {
      Profile* profile = browser()->profile();
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      // This error means that the Sync engine failed to initialize. Shutdown
      // Sync engine by revoking sync consent.
      identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
          signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      // Re-enable sync with the same primary account.
      signin_ui_util::EnableSyncFromSingleAccountPromo(
          profile,
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      break;
    }
    case AvatarSyncErrorType::kSyncPaused:
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          browser()->profile(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      break;
    case AvatarSyncErrorType::kUpgradeClientError:
      chrome::OpenUpdateChromeDialog(browser());
      break;
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError:
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError:
      OpenTabForSyncKeyRetrieval(
          browser(), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForEverythingError:
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForPasswordsError:
      OpenTabForSyncKeyRecoverabilityDegraded(
          browser(), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case AvatarSyncErrorType::kPassphraseError:
      if (base::FeatureList::IsEnabled(switches::kImprovedSigninUIOnDesktop)) {
        ShowSyncPassphraseDialog(
            *browser(), base::BindRepeating(
                            &SyncPassphraseDialogDecryptData,
                            base::Unretained(SyncServiceFactory::GetForProfile(
                                browser()->profile()))));
      } else {
        chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      }
      break;
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
  }
}

void ProfileMenuView::OnSigninButtonClicked(
    CoreAccountInfo account,
    ActionableItem button_type,
    signin_metrics::AccessPoint access_point) {
  RecordClick(button_type);

  if (!perform_menu_actions())
    return;
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);

  if (button_type == ActionableItem::kSigninReauthButton) {
    // The reauth button does not trigger a sync opt in.
    signin_ui_util::ShowReauthForAccount(browser()->profile(), account.email,
                                         access_point);
    return;
  }

  signin_ui_util::EnableSyncFromSingleAccountPromo(browser()->profile(),
                                                   account, access_point);
}

void ProfileMenuView::OnSignoutButtonClicked() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  DCHECK(ChromeSigninClientFactory::GetForProfile(browser()->profile())
             ->IsClearPrimaryAccountAllowed(identity_manager->HasPrimaryAccount(
                 signin::ConsentLevel::kSync)))
      << "Clear primary account is not allowed. Signout should not be offered "
         "in the UI.";

  RecordClick(ActionableItem::kSignoutButton);
  if (!perform_menu_actions())
    return;
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint::
          ACCESS_POINT_PROFILE_MENU_SIGNOUT_CONFIRMATION_PROMPT,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  RecordClick(ActionableItem::kOtherProfileButton);
  if (!perform_menu_actions())
    return;

  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    profiles::SwitchToProfile(profile_path, /*always_create=*/false);
  } else {
    // Open the same web app for another profile.
    // On non-macOS the only allowlisted case is PasswordManager WebApp, which
    // uses a different code path from other PWAs as it needs to not only
    // support switching profiles, but also possibly installing the app into a
    // different profile. Regular PWAs can only switch to profiles where the app
    // is already installed.
    const webapps::AppId& app_id = browser()->app_controller()->app_id();
#if BUILDFLAG(IS_MAC)
    if (app_id != web_app::kPasswordManagerAppId) {
      apps::AppShimManager::Get()->LaunchAppInProfile(app_id, profile_path);
      return;
    }
#endif
    CHECK_EQ(app_id, web_app::kPasswordManagerAppId);

    app_profile_switcher_.emplace(
        app_id, *browser()->profile(),
        base::BindOnce(
            [](views::Widget* widget) {
              widget->CloseWithReason(
                  views::Widget::ClosedReason::kUnspecified);
            },
            // It's safe to use base::Unretained, because the profile
            // switcher is owned by ProfileMenuView and is destroyed
            // before the widget is destroyed.
            base::Unretained(GetWidget())));
    app_profile_switcher_->SwitchToProfile(profile_path);
  }
}

void ProfileMenuView::OnAddNewProfileButtonClicked() {
  RecordClick(ActionableItem::kAddNewProfileButton);
  if (!perform_menu_actions())
    return;
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  RecordClick(ActionableItem::kManageProfilesButton);
  if (!perform_menu_actions())
    return;
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  RecordClick(ActionableItem::kEditProfileButton);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kManageProfileSubPage);
}

void ProfileMenuView::OnCookiesClearedOnExitLinkClicked() {
  RecordClick(ActionableItem::kCookiesClearedOnExitLink);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
}

void ProfileMenuView::BuildIdentity() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(account);
  ProfileAttributesEntry* profile_attributes =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!profile_attributes) {
    // May happen if the profile is being deleted. https://crbug.com/1040079
    return;
  }

  std::u16string profile_name;
  std::optional<EditButtonParams> edit_button_params;
  profile_name = profile_attributes->GetLocalProfileName();
  if (!web_app::AppBrowserController::IsWebApp(browser()) &&
      !switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    edit_button_params = EditButtonParams(
        &kEditChromeRefreshIcon,
        l10n_util::GetStringUTF16(
            IDS_PROFILES_CUSTOMIZE_PROFILE_BUTTON_TOOLTIP),
        base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                            base::Unretained(this)));
  }

  SkColor background_color =
      profile_attributes->GetProfileThemeColors().profile_highlight_color;
  if (!account_info.IsEmpty()) {
    menu_title_ = base::UTF8ToUTF16(account_info.full_name);
    menu_subtitle_ =
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.email);
    auto account_manager = chrome::GetAccountManagerIdentity(profile);
    std::u16string management_label;
    ui::ImageModel badge_image_model;

    if (enterprise_util::CanShowEnterpriseBadgingForMenu(
            browser()->profile())) {
      management_label =
          account_manager
              ? l10n_util::GetStringFUTF16(IDS_PROFILES_MANAGED_BY,
                                           base::UTF8ToUTF16(*account_manager))
              : std::u16string();
      if (auto* icon = policy::ManagementServiceFactory::GetForProfile(profile)
                           ->GetManagementIcon()) {
        badge_image_model = *icon;
      } else {
        badge_image_model = ui::ImageModel::FromVectorIcon(
            vector_icons::kBusinessIcon, ui::kColorMenuIcon, 16);
      }
    }

    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(account_info.account_image),
        badge_image_model, menu_title_, menu_subtitle_, management_label);
  } else {
    std::string profile_user_display_name, profile_user_email;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    profile_user_display_name = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserDisplayName);
    profile_user_email = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
#endif
    menu_title_ =
        profile_user_display_name.empty()
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(profile_user_display_name);
    // The email may be empty.
    menu_subtitle_ = base::UTF8ToUTF16(
        profile_user_email.empty() ? account_info.email : profile_user_email);

    std::u16string management_label;
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(
            // If the user is in the web-only signed-in state in the
            // experimental version of the UNO model, use the account image in
            // the profile menu header.
            // If the account does not have an image or it's not available yet,
            // a grey silhouette will be used.
            // If UNO is disabled or enabled in the full version, or there is no
            // account, use the profile icon.
            !account_info.IsEmpty()
                ? account_info.account_image
                : profile_attributes->GetAvatarIcon(kIdentityImageSize)),
        ui::ImageModel(), menu_title_, menu_subtitle_, management_label);
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  int guest_window_count = BrowserList::GetGuestBrowserCount();

  menu_title_ = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  menu_subtitle_ = std::u16string();
  std::u16string management_label;
  if (guest_window_count > 1) {
    menu_subtitle_ = l10n_util::GetPluralStringFUTF16(
        IDS_GUEST_WINDOW_COUNT_MESSAGE, guest_window_count);
  }

  ui::ThemedVectorIcon header_art_icon(&kGuestMenuArtIcon,
                                       ui::kColorAvatarHeaderArt);
  SetProfileIdentityInfo(
      /*profile_name=*/std::u16string(),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*edit_button=*/std::nullopt, profiles::GetGuestAvatar(),
      ui::ImageModel(), menu_title_, menu_subtitle_, management_label,
      header_art_icon);
}

void ProfileMenuView::BuildAutofillButtons() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  AddShortcutFeatureButton(
      vector_icons::kPasswordManagerIcon,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      kCreditCardChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      vector_icons::kLocationOnChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  Profile* profile = browser()->profile();
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed))
    return;

  if (!SyncServiceFactory::IsSyncAllowed(profile)) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_sync_feature_enabled =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  // First, check for sync errors. They may exist even if sync-the-feature is
  // disabled and only sync-the-transport is running.
  const std::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile);
  if (error) {
    BuildSyncInfoWithCallToAction(
        GetAvatarSyncErrorDescription(*error, is_sync_feature_enabled),
        GetSyncErrorButtonText(*error),
        base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                            base::Unretained(this), *error),
        /*show_sync_badge=*/is_sync_feature_enabled);
    return;
  }

  // If there's no error and sync-the-feature is enabled, the text says
  // everything is fine and the button simply opens sync settings.
  if (is_sync_feature_enabled) {
    BuildSyncInfoWithoutCallToAction(
        l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                            base::Unretained(this)));
    return;
  }

  // If there's no error and sync-the-feature is disabled, show a sync promo.
  // For a signed-in user, the promo just opens the "turn on sync" dialog.
  // For a web-only signed-in user in the UNO model, the promo signs the user on
  // Chrome and opens the "turn on sync" dialog.
  // For a signed-out user, it prompts for sign-in first.
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info_for_promos =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  std::u16string description;
  std::u16string button_text;
  ActionableItem button_type = ActionableItem::kSigninAccountButton;
  bool show_sync_badge = false;
  bool show_account_card = false;
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN;

  if (!account_info.IsEmpty()) {
    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info.account_id)) {
      // Sign-in pending state.
      button_type = ActionableItem::kSigninReauthButton;
      description =
          l10n_util::GetStringUTF16(IDS_SIGNIN_PAUSED_USER_MENU_VERIFY_MESSAGE);
      button_text =
          l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON);
    } else {
      // Signed-in not-syncing state.
      description = l10n_util::GetStringUTF16(
          switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
              ? IDS_PROFILES_DICE_SYNC_PROMO
              : IDS_PROFILES_DICE_NOT_SYNCING_TITLE);
      button_text = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
      show_sync_badge = !switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
    }
  } else if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
             !account_info_for_promos.IsEmpty()) {
    // Web-only signed-in state.
    account_info = account_info_for_promos;
    access_point = signin_metrics::AccessPoint::
        ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO;
    description = l10n_util::GetStringUTF16(
        switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
            ? IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION
            : IDS_PROFILES_DICE_SYNC_PROMO);
    button_text = l10n_util::GetStringFUTF16(
        IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
        base::UTF8ToUTF16(!account_info_for_promos.given_name.empty()
                              ? account_info_for_promos.given_name
                              : account_info_for_promos.email));
    button_type = ActionableItem::kEnableSyncForWebOnlyAccountButton;
    show_account_card = true;
  } else {
    // Not signed in state.
    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      access_point = signin_metrics::AccessPoint::
          ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO;
      description =
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION);
      button_text =
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON);
    } else {
      description = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO);
      button_text = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
    }
    button_type = ActionableItem::kSigninButton;
  }

  CHECK(!description.empty());
  CHECK(!button_text.empty());
  BuildSyncInfoWithCallToAction(
      description, button_text,
      base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                          base::Unretained(this), account_info, button_type,
                          access_point),
      show_sync_badge,
      show_account_card ? account_info_for_promos : AccountInfo());
}

std::u16string ProfileMenuView::GetIdentitySectionSubtitle(
    const CoreAccountInfo& account_info) const {
  // TODO(crbug.com/356603651): Implement this.
  return account_info.email.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION)
             : base::UTF8ToUTF16(account_info.email);
}

void ProfileMenuView::BuildIdentityWithCallToAction() {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* profile_attributes =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!profile_attributes) {
    // May happen if the profile is being deleted. https://crbug.com/1040079
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(account);

  // TODO(crbug.com/356603651): Finish button implementation.
  std::u16string button_text =
      account.IsEmpty()
          ? l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON)
          : std::u16string();

  profiles::PlaceholderAvatarIconParams icon_params = {.has_padding = true,
                                                       .has_background = false};
  ui::ImageModel profile_image = ui::ImageModel::FromImage(
      !account_info.IsEmpty() ? account_info.account_image
                              : profile_attributes->GetAvatarIcon(
                                    kIdentityInfoImageSize,
                                    /*use_high_res_file=*/true, icon_params));
  std::u16string title = GetProfileIdentifier(
      profile_attributes->GetLocalProfileName(), account_info.given_name);

  SetProfileIdentityWithCallToAction(
      profile_attributes->GetProfileThemeColors().profile_highlight_color,
      profile_image, title, GetIdentitySectionSubtitle(account), button_text,
      /*button_image=*/ui::ImageModel(), base::DoNothing());
}

void ProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool has_sync_consent =
      identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  bool has_unconsented_account = HasUnconstentedProfile(profile);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
      !profile->IsGuestSession()) {
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_CUSTOMIZE_PROFILE_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                            base::Unretained(this)),
        vector_icons::kEditChromeRefreshIcon);

    // Show the settings button when signed in to Chrome or to the web. Do not
    // show if sync is enabled.
    bool should_show_settings_button =
        !has_sync_consent && identity_manager &&
        !identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken()
             .empty();

    if (should_show_settings_button) {
      AddFeatureButton(
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_OPEN_ACCOUNT_SETTINGS),
          base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                              base::Unretained(this)),
          vector_icons::kSettingsChromeRefreshIcon);
    }
  }

  if (has_unconsented_account && !IsSyncPaused(profile)) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // The Google G icon needs to be shrunk, so it won't look too big compared
    // to the other icons.
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
        base::BindRepeating(
            &ProfileMenuView::OnManageGoogleAccountButtonClicked,
            base::Unretained(this)),
        vector_icons::kGoogleGLogoIcon,
        /*icon_to_image_ratio=*/0.75f);
#else
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
        base::BindRepeating(
            &ProfileMenuView::OnManageGoogleAccountButtonClicked,
            base::Unretained(this)));
#endif
  }

  int window_count = CountBrowsersFor(profile);
  if (profile->IsGuestSession()) {
    AddFeatureButton(
        l10n_util::GetPluralStringFUTF16(IDS_GUEST_PROFILE_MENU_CLOSE_BUTTON,
                                         window_count),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)),
        vector_icons::kCloseChromeRefreshIcon);
  } else if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
             window_count > 0) {
    AddFeatureButton(
        l10n_util::GetPluralStringFUTF16(
            IDS_PROFILE_MENU_CLOSE_PROFILE_X_WINDOWS_BUTTON, window_count),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)),
        vector_icons::kCloseChromeRefreshIcon);
  } else if (window_count > 1) {
    AddFeatureButton(
        l10n_util::GetPluralStringFUTF16(IDS_PROFILES_CLOSE_X_WINDOWS_BUTTON,
                                         window_count),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)),
        vector_icons::kCloseChromeRefreshIcon);
  }

  const bool has_primary_account =
      !profile->IsGuestSession() && has_sync_consent;

  bool hide_signout_button_for_managed_profiles =
      enterprise_util::UserAcceptedAccountManagement(profile) &&
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout);

  bool add_sign_out_button = has_unconsented_account && !has_primary_account &&
                             !hide_signout_button_for_managed_profiles;
  // The sign-out button is always at the bottom.
  if (add_sign_out_button) {
    std::u16string signout_button_text;
    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      // Note: Sign out button is only added if there is a signed profile with
      // no sync consent, so there is no need to check these conditions for the
      // sign in pending state.
      bool signin_pending =
          identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              identity_manager->GetPrimaryAccountId(
                  signin::ConsentLevel::kSignin));
      signout_button_text = l10n_util::GetStringUTF16(
          signin_pending ? IDS_PROFILE_MENU_SIGN_OUT_WHEN_SIGNIN_PENDING
                         : IDS_PROFILE_MENU_SIGN_OUT);
    } else {
      signout_button_text = l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT);
    }
    AddFeatureButton(
        signout_button_text,
        base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                            base::Unretained(this)),
        kSignOutIcon);
  }
}

void ProfileMenuView::BuildAvailableProfiles() {
  bool profiles_selectable = true;
#if BUILDFLAG(IS_MAC)
  const bool is_regular_web_app =
      web_app::AppBrowserController::IsWebApp(browser()) &&
      (browser()->app_controller()->app_id() != web_app::kPasswordManagerAppId);
  std::set<base::FilePath> available_profiles;
  if (is_regular_web_app) {
    available_profiles = AppShimRegistry::Get()->GetInstalledProfilesForApp(
        browser()->app_controller()->app_id());
  }
#endif

  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByNameWithCheck();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath()) {
      continue;
    }
    if (profile_entry->IsOmitted()) {
      continue;
    }

#if BUILDFLAG(IS_MAC)
    if (is_regular_web_app &&
        !available_profiles.contains(profile_entry->GetPath())) {
      continue;
    }
#endif

    AddAvailableProfile(
        ui::ImageModel::FromImage(profile_entry->GetAvatarIcon(
            profiles::kMenuAvatarIconSize, /*use_high_res_file=*/true,
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                browser()->window()->GetColorProvider()->GetColor(
                    ui::kColorMenuBackground)))),
        profile_entry->GetName(),
        /*is_guest=*/false,
        /*is_enabled=*/profiles_selectable,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }
  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        profile_entries.size() > 1);

  if (!browser()->profile()->IsGuestSession() &&
      profiles::IsGuestModeEnabled(*browser()->profile()) &&
      !web_app::AppBrowserController::IsWebApp(browser())) {
    AddAvailableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(
            switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
                ? IDS_PROFILE_MENU_OPEN_GUEST_PROFILE
                : IDS_GUEST_PROFILE_NAME),
        /*is_guest=*/true,
        /*is_enabled=*/true,
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  bool profiles_selectable = true;
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    if (profiles_selectable || profiles::IsProfileCreationAllowed()) {
      AddProfileManagementFeaturesSeparator();
    }

    if (profiles::IsProfileCreationAllowed()) {
      AddProfileManagementFeatureButton(
          kAccountAddChromeRefreshIcon,
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_ADD_NEW_PROFILE),
          base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                              base::Unretained(this)));
    }

    if (profiles_selectable) {
      AddProfileManagementFeatureButton(
          kAccountManageChromeRefreshIcon,
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_MANAGE_PROFILES),
          base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                              base::Unretained(this)));
    } else {
      AddProfileManagementManagedHint(
          vector_icons::kBusinessIcon,
          l10n_util::GetStringUTF16(
              IDS_PROFILES_MANAGE_PROFILES_MANAGED_TOOLTIP));
    }
  } else {
    if (profiles_selectable) {
      AddProfileManagementShortcutFeatureButton(
          vector_icons::kSettingsChromeRefreshIcon,
          l10n_util::GetStringUTF16(
              IDS_PROFILES_MANAGE_PROFILES_BUTTON_TOOLTIP),
          base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                              base::Unretained(this)));
    } else {
      AddProfileManagementManagedHint(
          vector_icons::kBusinessIcon,
          l10n_util::GetStringUTF16(
              IDS_PROFILES_MANAGE_PROFILES_MANAGED_TOOLTIP));
    }
    if (profiles::IsProfileCreationAllowed()) {
      AddProfileManagementFeatureButton(
          vector_icons::kAddIcon, l10n_util::GetStringUTF16(IDS_ADD),
          base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                              base::Unretained(this)));
    }
  }
}
