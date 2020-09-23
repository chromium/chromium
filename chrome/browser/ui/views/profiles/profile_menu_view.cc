// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

// Helpers --------------------------------------------------------------------

// Most error cases get displayed the same way, only the button string changes.
ProfileMenuViewBase::SyncInfo GetStandardSyncErrorInfo(int button_string_id) {
  return {IDS_SYNC_ERROR_USER_MENU_TITLE, button_string_id,
          ProfileMenuViewBase::SyncInfoContainerBackgroundState::kError};
}

ProfileMenuViewBase::SyncInfo GetSyncInfoForAvatarErrorType(
    sync_ui_util::AvatarSyncErrorType error) {
  switch (error) {
    case sync_ui_util::NO_SYNC_ERROR:
      return {IDS_SETTINGS_EMPTY_STRING, IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON,
              ProfileMenuViewBase::SyncInfoContainerBackgroundState::kNoError};
    case sync_ui_util::AUTH_ERROR:
      // Sync paused. The user can reauth to resolve the signin error.
      return {IDS_PROFILES_DICE_SYNC_PAUSED_TITLE,
              IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON,
              ProfileMenuViewBase::SyncInfoContainerBackgroundState::kPaused};
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
      // For a managed user, the user is directed to the signout confirmation
      // dialogue in the settings page.
      return GetStandardSyncErrorInfo(IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON);
    case sync_ui_util::UNRECOVERABLE_ERROR:
      // For a non-managed user, we sign out on the user's behalf and prompt the
      // user to sign in again.
      return GetStandardSyncErrorInfo(
          IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_BUTTON);
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      return GetStandardSyncErrorInfo(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON);
    case sync_ui_util::PASSPHRASE_ERROR:
      return GetStandardSyncErrorInfo(
          IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON);
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR:
      return GetStandardSyncErrorInfo(
          IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_BUTTON);
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR:
      return {IDS_SYNC_ERROR_PASSWORDS_USER_MENU_TITLE,
              IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_BUTTON,
              ProfileMenuViewBase::SyncInfoContainerBackgroundState::kError};
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      return GetStandardSyncErrorInfo(
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
  }

  NOTREACHED();
  return {IDS_SETTINGS_EMPTY_STRING, IDS_SETTINGS_EMPTY_STRING,
          ProfileMenuViewBase::SyncInfoContainerBackgroundState::kNoError};
}

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  CHECK(g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath(), &entry));
  return entry;
}

void NavigateToGoogleAccountPage(Profile* profile, const std::string& email) {
  // Create a URL so that the account chooser is shown if the account with
  // |email| is not signed into the web. Include a UTM parameter to signal the
  // source of the navigation.
  GURL google_account = net::AppendQueryParameter(
      GURL(chrome::kGoogleAccountURL), "utm_source", "chrome-profile-chooser");

  GURL url(chrome::kGoogleAccountChooserURL);
  url = net::AppendQueryParameter(url, "Email", email);
  url = net::AppendQueryParameter(url, "continue", google_account.spec());

  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

// Returns the number of browsers associated with |profile|.
// Note: For regular profiles this includes incognito sessions.
int CountBrowsersFor(Profile* profile) {
  int browser_count = chrome::GetBrowserCount(profile);
  if (!profile->IsOffTheRecord() && profile->HasPrimaryOTRProfile())
    browser_count += chrome::GetBrowserCount(profile->GetPrimaryOTRProfile());
  return browser_count;
}

bool IsSyncPaused(Profile* profile) {
  return sync_ui_util::GetAvatarSyncErrorType(profile) ==
         sync_ui_util::AUTH_ERROR;
}

}  // namespace

// ProfileMenuView ---------------------------------------------------------

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(views::Button* anchor_button, Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
  set_close_on_deactivate(close_on_deactivate_for_testing_);
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::BuildMenu() {
  Profile* profile = browser()->profile();
  const bool is_guest = profile->IsGuestSession();
  if (profile->IsRegularProfile()) {
    BuildIdentity();
    BuildSyncInfo();
    BuildAutofillButtons();
  } else if (is_guest) {
    BuildGuestIdentity();
  } else {
    NOTREACHED();
  }

  BuildFeatureButtons();

//  ChromeOS doesn't support multi-profile.
#if !defined(OS_CHROMEOS)
  if (!(is_guest &&
        base::FeatureList::IsEnabled(features::kNewProfilePicker))) {
    BuildProfileManagementHeading();
    BuildSelectableProfiles();
    BuildProfileManagementFeatureButtons();
  }
#endif
}

gfx::ImageSkia ProfileMenuView::GetSyncIcon() const {
  Profile* profile = browser()->profile();

  if (!profile->IsRegularProfile())
    return gfx::ImageSkia();

  if (!IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount())
    return ColoredImageForMenu(kSyncPausedCircleIcon, gfx::kGoogleGrey500);

  const gfx::VectorIcon* icon = nullptr;
  ui::NativeTheme::ColorId color_id;
  switch (sync_ui_util::GetAvatarSyncErrorType(profile)) {
    case sync_ui_util::NO_SYNC_ERROR:
      icon = &kSyncCircleIcon;
      color_id = ui::NativeTheme::kColorId_AlertSeverityLow;
      break;
    case sync_ui_util::AUTH_ERROR:
      icon = &kSyncPausedCircleIcon;
      color_id = ui::NativeTheme::kColorId_ProminentButtonColor;
      break;
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
    case sync_ui_util::UNRECOVERABLE_ERROR:
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      icon = &kSyncPausedCircleIcon;
      color_id = ui::NativeTheme::kColorId_AlertSeverityHigh;
      break;
  }
  const SkColor image_color = GetNativeTheme()->GetSystemColor(color_id);
  return ColoredImageForMenu(*icon, image_color);
}

base::string16 ProfileMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  RecordClick(ActionableItem::kManageGoogleAccountButton);
  if (!perform_menu_actions())
    return;

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kNotRequired));
  NavigateToGoogleAccountPage(
      profile, identity_manager
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
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
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
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

void ProfileMenuView::OnSyncErrorButtonClicked(
    sync_ui_util::AvatarSyncErrorType error) {
#if defined(OS_CHROMEOS)
  // On ChromeOS, sync errors are fixed by re-signing into the OS.
  chrome::AttemptUserExit();
#else
  RecordClick(ActionableItem::kSyncErrorButton);
  if (!perform_menu_actions())
    return;

  // The logic below must be consistent with GetSyncInfoForAvatarErrorType().
  switch (error) {
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
      chrome::ShowSettingsSubPage(browser(), chrome::kSignOutSubPage);
      break;
    case sync_ui_util::UNRECOVERABLE_ERROR:
      if (ProfileSyncServiceFactory::GetForProfile(browser()->profile())) {
        syncer::RecordSyncEvent(syncer::STOP_FROM_OPTIONS);
      }

      // GetPrimaryAccountMutator() might return nullptr on some platforms.
      if (auto* account_mutator =
              IdentityManagerFactory::GetForProfile(browser()->profile())
                  ->GetPrimaryAccountMutator()) {
        account_mutator->ClearPrimaryAccount(
            signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
            signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
        Hide();
        browser()->signin_view_controller()->ShowSignin(
            profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN,
            signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      }
      break;
    case sync_ui_util::AUTH_ERROR:
      Hide();
      browser()->signin_view_controller()->ShowSignin(
          profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH,
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      break;
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      chrome::OpenUpdateChromeDialog(browser());
      break;
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR:
      sync_ui_util::OpenTabForSyncKeyRetrieval(
          browser(), syncer::KeyRetrievalTriggerForUMA::kProfileMenu);
      break;
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
    case sync_ui_util::NO_SYNC_ERROR:
      NOTREACHED();
      break;
  }
#endif
}

void ProfileMenuView::OnSigninAccountButtonClicked(AccountInfo account) {
  RecordClick(ActionableItem::kSigninAccountButton);
  if (!perform_menu_actions())
    return;
  Hide();
  signin_ui_util::EnableSyncFromPromo(
      browser(), account,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      true /* is_default_promo_account */);
}

#if !defined(OS_CHROMEOS)
void ProfileMenuView::OnSignoutButtonClicked() {
  RecordClick(ActionableItem::kSignoutButton);
  if (!perform_menu_actions())
    return;
  Hide();
  // Sign out from all accounts.
  browser()->signin_view_controller()->ShowGaiaLogoutTab(
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnSigninButtonClicked() {
  RecordClick(ActionableItem::kSigninButton);
  if (!perform_menu_actions())
    return;
  Hide();
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  RecordClick(ActionableItem::kOtherProfileButton);
  if (!perform_menu_actions())
    return;
  Hide();
  profiles::SwitchToProfile(profile_path, /*always_create=*/false,
                            ProfileManager::CreateCallback());
}

void ProfileMenuView::OnAddNewProfileButtonClicked() {
  RecordClick(ActionableItem::kAddNewProfileButton);
  if (!perform_menu_actions())
    return;
  UserManager::Show(/*profile_path_to_focus=*/base::FilePath(),
                    profiles::USER_MANAGER_OPEN_CREATE_USER_PAGE);
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  RecordClick(ActionableItem::kManageProfilesButton);
  if (!perform_menu_actions())
    return;
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  RecordClick(ActionableItem::kEditProfileButton);
  if (!perform_menu_actions())
    return;
  chrome::ShowSettingsSubPage(browser(), chrome::kManageProfileSubPage);
}
#endif  // defined(OS_CHROMEOS)

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
  CoreAccountInfo account = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          account);
  ProfileAttributesEntry* profile_attributes =
      GetProfileAttributesEntry(profile);

  base::string16 profile_name;
  base::Optional<EditButtonParams> edit_button_params;
// Profile names are not supported on ChromeOS.
#if !defined(OS_CHROMEOS)
  size_t num_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  if (num_of_profiles > 1 || !profile_attributes->IsUsingDefaultName() ||
      base::FeatureList::IsEnabled(features::kNewProfilePicker)) {
    profile_name = profile_attributes->GetLocalProfileName();
    edit_button_params = EditButtonParams(
        &vector_icons::kEditIcon,
        l10n_util::GetStringUTF16(IDS_SETTINGS_EDIT_PERSON),
        base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                            base::Unretained(this)));
  }
#endif

  SkColor background_color =
      GetThemeColorsForProfile(profile).profile_highlight_color;
  if (account_info.has_value()) {
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(account_info.value().account_image),
        base::UTF8ToUTF16(account_info.value().full_name),
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.value().email));
  } else {
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(
            profile_attributes->GetAvatarIcon(kIdentityImageSize)),
        /*title=*/base::string16(),
        l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  int guest_window_count = BrowserList::GetGuestBrowserCount();

  base::string16 subtitle;
  if (guest_window_count > 1 &&
      base::FeatureList::IsEnabled(features::kNewProfilePicker)) {
    subtitle = l10n_util::GetPluralStringFUTF16(IDS_GUEST_WINDOW_COUNT_MESSAGE,
                                                guest_window_count);
  }

  // TODO(crbug.com/1105763): Add asset colors to native theme and update icon
  // temporary color placeholder to align with the design deck colors.
  ui::ThemedVectorIcon header_art_icon(&kGuestMenuArtIcon,
                                       ui::NativeTheme::kColorId_MenuIconColor);
  SetProfileIdentityInfo(
      /*profile_name=*/base::string16(),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*edit_button=*/base::nullopt, profiles::GetGuestAvatar(),
      l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME), subtitle,
      header_art_icon);
}

void ProfileMenuView::BuildAutofillButtons() {
  AddShortcutFeatureButton(
      kKeyIcon, l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      kCreditCardIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      vector_icons::kLocationOnIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
  Profile* profile = browser()->profile();
  // Only show the sync info if signin and sync are allowed.
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      !ProfileSyncServiceFactory::IsSyncAllowed(profile)) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager->HasPrimaryAccount()) {
    // Show sync state.
    const sync_ui_util::AvatarSyncErrorType error =
        sync_ui_util::GetAvatarSyncErrorType(browser()->profile());

    const base::RepeatingClosure action =
        error == sync_ui_util::NO_SYNC_ERROR
            ? base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                                  base::Unretained(this))
            : base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                                  base::Unretained(this), error);
    SetSyncInfo(GetSyncInfoForAvatarErrorType(error), action,
                /*show_badge=*/true);

    return;
  }

  // Show sync promos.
  CoreAccountInfo unconsented_account = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          unconsented_account);

  if (account_info.has_value()) {
    SetSyncInfo(
        {IDS_PROFILES_DICE_NOT_SYNCING_TITLE, IDS_PROFILES_DICE_SIGNIN_BUTTON,
         SyncInfoContainerBackgroundState::kNoPrimaryAccount},
        base::BindRepeating(&ProfileMenuView::OnSigninAccountButtonClicked,
                            base::Unretained(this), account_info.value()),
        /*show_badge=*/true);
  } else {
#if defined(OS_CHROMEOS)
    // There is always an account on ChromeOS.
    NOTREACHED();
#else
    SetSyncInfo({IDS_PROFILES_DICE_SYNC_PROMO, IDS_PROFILES_DICE_SIGNIN_BUTTON,
                 SyncInfoContainerBackgroundState::kNoPrimaryAccount},
                base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                                    base::Unretained(this)),
                /*show_badge=*/false);
#endif
  }
}

void ProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool is_guest = profile->IsGuestSession();
  const bool has_unconsented_account =
      !is_guest &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kNotRequired);

  if (has_unconsented_account && !IsSyncPaused(profile)) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // The Google G icon needs to be shrunk, so it won't look too big compared
    // to the other icons.
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
        base::BindRepeating(
            &ProfileMenuView::OnManageGoogleAccountButtonClicked,
            base::Unretained(this)),
        kGoogleGLogoIcon,
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
  if (base::FeatureList::IsEnabled(features::kNewProfilePicker) && is_guest) {
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

#if !defined(OS_CHROMEOS)
  const bool has_primary_account =
      !is_guest && identity_manager->HasPrimaryAccount();
  // The sign-out button is always at the bottom.
  if (has_unconsented_account && !has_primary_account) {
    AddFeatureButton(
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
        base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                            base::Unretained(this)),
        kSignOutIcon);
  }
#endif
}

#if !defined(OS_CHROMEOS)
void ProfileMenuView::BuildProfileManagementHeading() {
  SetProfileManagementHeading(
      l10n_util::GetStringUTF16(IDS_PROFILES_OTHER_PROFILES_TITLE));
}

void ProfileMenuView::BuildSelectableProfiles() {
  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByName();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath())
      continue;

    AddSelectableProfile(
        ui::ImageModel::FromImage(
            profile_entry->GetAvatarIcon(kSelectableProfileImageSize)),
        profile_entry->GetName(),
        /*is_guest=*/false,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }
  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        profile_entries.size() > 1);

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (!browser()->profile()->IsGuestSession() &&
      service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
    AddSelectableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME),
        /*is_guest=*/true,
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  AddProfileManagementShortcutFeatureButton(
      vector_icons::kSettingsIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_USERS_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                          base::Unretained(this)));

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (service->GetBoolean(prefs::kBrowserAddPersonEnabled)) {
    AddProfileManagementFeatureButton(
        kAddIcon, l10n_util::GetStringUTF16(IDS_ADD),
        base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                            base::Unretained(this)));
  }
}
#endif  // defined(OS_CHROMEOS)
