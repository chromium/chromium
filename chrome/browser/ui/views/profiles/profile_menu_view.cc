// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/browser/signin/signin_features.h"
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
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_view_utils.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"

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
    BuildIdentity();

    // Users should not be able to open chrome settings from WebApps.
    if (!web_app::AppBrowserController::IsWebApp(browser())) {
      BuildSyncInfo();
      BuildAutofillButtons();
    }
  }

  // Users should not be able to use features from WebApps.
  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    BuildFeatureButtons();
  }

//  ChromeOS doesn't support multi-profile.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!(profile->IsGuestSession())) {
    SetProfileManagementHeading(
        l10n_util::GetStringUTF16(IDS_PROFILES_LIST_PROFILES_TITLE));
    BuildAvailableProfiles();

    // Users should not be able to manage profiles from WebApps.
    if (!web_app::AppBrowserController::IsWebApp(browser())) {
      BuildProfileManagementFeatureButtons();
    }
  }
#endif

  if (web_app::AppBrowserController::IsWebApp(browser())) {
    browser()->window()->NotifyFeatureEngagementEvent(
        "web_app_profile_menu_shown");
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
    return features::IsChromeRefresh2023()
               ? ColoredImageForMenu(kSyncDisabledChromeRefreshIcon,
                                     kColorProfileMenuSyncOffIcon)
               : ColoredImageForMenu(kSyncPausedCircleIcon, ui::kColorIcon);
  }

  absl::optional<AvatarSyncErrorType> error = GetAvatarSyncErrorType(profile);
  if (!error) {
    return features::IsChromeRefresh2023()
               ? ColoredImageForMenu(kSyncChromeRefreshIcon,
                                     kColorProfileMenuSyncIcon)
               : ColoredImageForMenu(kSyncCircleIcon,
                                     ui::kColorAlertLowSeverity);
  }

  ui::ColorId color_id = error == AvatarSyncErrorType::kSyncPaused
                             ? ui::kColorButtonBackgroundProminent
                             : ui::kColorAlertHighSeverity;
  ui::ColorId refreshed_color_id = error == AvatarSyncErrorType::kSyncPaused
                                       ? kColorProfileMenuSyncPausedIcon
                                       : kColorProfileMenuSyncErrorIcon;
  return features::IsChromeRefresh2023()
             ? ColoredImageForMenu(kSyncDisabledChromeRefreshIcon,
                                   refreshed_color_id)
             : ColoredImageForMenu(kSyncPausedCircleIcon, color_id);
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
  DCHECK(profiles::IsGuestModeEnabled());
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, sync errors are fixed by re-signing into the OS.
  chrome::AttemptUserExit();
#else
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
          signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
          signin_metrics::SignoutDelete::kIgnoreMetric);
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
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
  }
#endif
}

void ProfileMenuView::OnSigninAccountButtonClicked(CoreAccountInfo account) {
  RecordClick(ActionableItem::kSigninAccountButton);
  if (!perform_menu_actions())
    return;
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      browser()->profile(), account,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Sign out from all accounts.
  browser()->signin_view_controller()->ShowGaiaLogoutTab(
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);
#else
  CHECK(!browser()->profile()->IsMainProfile());
  identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SignoutDelete::kIgnoreMetric);
#endif
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileMenuView::OnSigninButtonClicked() {
  RecordClick(ActionableItem::kSigninButton);
  if (!perform_menu_actions())
    return;
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);

  signin_ui_util::EnableSyncFromSingleAccountPromo(
      browser()->profile(), AccountInfo(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
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
#if !BUILDFLAG(IS_CHROMEOS)
    // Open the same web app for another profile.
    // So far the only allowlisted case is PasswordManager WebApp.
    const web_app::AppId& app_id = browser()->app_controller()->app_id();
    CHECK_EQ(app_id, web_app::kPasswordManagerAppId);

    app_profile_switcher_.emplace(
        app_id, *browser()->profile(),
        base::BindOnce(
            [](views::Widget* widget) {
              widget->CloseWithReason(
                  views::Widget::ClosedReason::kUnspecified);
            },
            // It's safe to use base::Unretained, because the profile
            // switcher is onwned by ProfileMenuView and is destroyed
            // before the widget is destroyed.
            base::Unretained(GetWidget())));
    app_profile_switcher_->SwitchToProfile(profile_path);
#else
    // WebApps are can only be installed for the main profile on ChromeOS.
    NOTREACHED();
#endif
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ProfileMenuView::OnCookiesClearedOnExitLinkClicked() {
  RecordClick(ActionableItem::kCookiesClearedOnExitLink);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
}

void ProfileMenuView::BuildIdentity() {
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
  absl::optional<EditButtonParams> edit_button_params;
// Profile names are not supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  profile_name = profile_attributes->GetLocalProfileName();
  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    edit_button_params = EditButtonParams(
        features::IsChromeRefresh2023() ? &kEditChromeRefreshIcon
                                        : &vector_icons::kEditIcon,
        l10n_util::GetStringUTF16(
            IDS_PROFILES_CUSTOMIZE_PROFILE_BUTTON_TOOLTIP),
        base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                            base::Unretained(this)));
  }
#endif

  SkColor background_color =
      profile_attributes->GetProfileThemeColors().profile_highlight_color;
  if (!account_info.IsEmpty()) {
    menu_title_ = base::UTF8ToUTF16(account_info.full_name);
    menu_subtitle_ =
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.email);
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(account_info.account_image), menu_title_,
        menu_subtitle_);
  } else {
    menu_title_ = std::u16string();
    menu_subtitle_ =
        l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(
            profile_attributes->GetAvatarIcon(kIdentityImageSize)),
        menu_title_, menu_subtitle_);
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  int guest_window_count = BrowserList::GetGuestBrowserCount();

  menu_title_ = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  menu_subtitle_ = std::u16string();
  if (guest_window_count > 1) {
    menu_subtitle_ = l10n_util::GetPluralStringFUTF16(
        IDS_GUEST_WINDOW_COUNT_MESSAGE, guest_window_count);
  }

  ui::ThemedVectorIcon header_art_icon(&kGuestMenuArtIcon,
                                       ui::kColorAvatarHeaderArt);
  SetProfileIdentityInfo(
      /*profile_name=*/std::u16string(),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*edit_button=*/absl::nullopt, profiles::GetGuestAvatar(), menu_title_,
      menu_subtitle_, header_art_icon);
}

void ProfileMenuView::BuildAutofillButtons() {
  AddShortcutFeatureButton(
      features::IsChromeRefresh2023() ? kKeyChromeRefreshIcon : kKeyIcon,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      features::IsChromeRefresh2023() ? kCreditCardChromeRefreshIcon
                                      : kCreditCardIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      features::IsChromeRefresh2023()
          ? vector_icons::kLocationOnChromeRefreshIcon
          : vector_icons::kLocationOnIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
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
  const absl::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile);
  if (error) {
    BuildSyncInfoWithCallToAction(
        GetAvatarSyncErrorDescription(*error, is_sync_feature_enabled),
        GetSyncErrorButtonText(*error),
        error == AvatarSyncErrorType::kSyncPaused
            ? ui::kColorSyncInfoBackgroundPaused
            : ui::kColorSyncInfoBackgroundError,
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
  // For a signed-out user, it prompts for sign-in first.
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!account_info.IsEmpty()) {
    BuildSyncInfoWithCallToAction(
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
        ui::kColorSyncInfoBackground,
        base::BindRepeating(&ProfileMenuView::OnSigninAccountButtonClicked,
                            base::Unretained(this), account_info),
        /*show_sync_badge=*/true);
  } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // There is always an account on ChromeOS.
    NOTREACHED_NORETURN();
#else
    BuildSyncInfoWithCallToAction(
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
        ui::kColorSyncInfoBackground,
        base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                            base::Unretained(this)),
        /*show_sync_badge=*/false);
#endif
  }
}

void ProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser()->profile();
  bool has_unconsented_account = HasUnconstentedProfile(profile);
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
        vector_icons::kCloseIcon);
  } else {
    if (window_count > 1) {
      AddFeatureButton(
          l10n_util::GetPluralStringFUTF16(IDS_PROFILES_CLOSE_X_WINDOWS_BUTTON,
                                           window_count),
          base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                              base::Unretained(this)),
          vector_icons::kCloseIcon);
    }
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool has_primary_account =
      !profile->IsGuestSession() &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);

  bool hide_signout_button_for_managed_profiles =
      chrome::enterprise_util::UserAcceptedAccountManagement(profile) &&
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout);

  bool add_sign_out_button = has_unconsented_account && !has_primary_account &&
                             !hide_signout_button_for_managed_profiles;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Clearing the primary account is not allowed in the main profile.
  add_sign_out_button &= !profile->IsMainProfile();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  // The sign-out button is always at the bottom.
  if (add_sign_out_button) {
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
        base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                            base::Unretained(this)),
        kSignOutIcon);
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileMenuView::BuildAvailableProfiles() {
  bool profiles_selectable = true;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  profiles_selectable = profiles::AreSecondaryProfilesAllowed();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByName();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath())
      continue;
    if (profile_entry->IsOmitted())
      continue;

    AddAvailableProfile(
        ui::ImageModel::FromImage(
            profile_entry->GetAvatarIcon(profiles::kMenuAvatarIconSize)),
        profile_entry->GetName(),
        /*is_guest=*/false,
        /*is_enabled=*/profiles_selectable,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }
  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        profile_entries.size() > 1);

  if (!browser()->profile()->IsGuestSession() &&
      profiles::IsGuestModeEnabled() &&
      !web_app::AppBrowserController::IsWebApp(browser())) {
    AddAvailableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME),
        /*is_guest=*/true,
        /*is_enabled=*/true,
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  bool profiles_selectable = true;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  profiles_selectable = profiles::AreSecondaryProfilesAllowed();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (profiles_selectable) {
    AddProfileManagementShortcutFeatureButton(
        features::IsChromeRefresh2023()
            ? vector_icons::kSettingsChromeRefreshIcon
            : vector_icons::kSettingsIcon,
        l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_PROFILES_BUTTON_TOOLTIP),
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
        kAddIcon, l10n_util::GetStringUTF16(IDS_ADD),
        base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                            base::Unretained(this)));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BEGIN_METADATA(ProfileMenuView, ProfileMenuViewBase)
ADD_READONLY_PROPERTY_METADATA(gfx::ImageSkia, SyncIcon)
END_METADATA
