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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
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
#include "components/signin/public/identity_manager/accounts_mutator.h"
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

constexpr float kShortcutIconToImageRatio = 9.0 / 16.0;

// Number of times the Dice sign-in promo illustration should be shown.
constexpr int kDiceSigninPromoIllustrationShowCountMax = 10;

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  CHECK(g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath(), &entry));
  return entry;
}

BadgedProfilePhoto::BadgeType GetProfileBadgeType(Profile* profile) {
  if (profile->IsSupervised()) {
    return profile->IsChild() ? BadgedProfilePhoto::BADGE_TYPE_CHILD
                              : BadgedProfilePhoto::BADGE_TYPE_SUPERVISOR;
  }
  // |Profile::IsSyncAllowed| is needed to check whether sync is allowed by GPO
  // policy.
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      profile->IsSyncAllowed() &&
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount()) {
    return BadgedProfilePhoto::BADGE_TYPE_SYNC_COMPLETE;
  }
  return BadgedProfilePhoto::BADGE_TYPE_NONE;
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

bool AreSigninCookiesClearedOnExit(Profile* profile) {
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
  return client->AreSigninCookiesDeletedOnExit();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns the Google G icon in grey and with a padding of 2. The padding is
// needed to make the icon look smaller, otherwise it looks too big compared to
// the other icons. See crbug.com/951751 for more information.
gfx::ImageSkia GetGoogleIconForUserMenu(int icon_size) {
  constexpr gfx::Insets kIconPadding = gfx::Insets(2);
  SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);
  // |CreateVectorIcon()| doesn't override colors specified in the .icon file,
  // therefore the image has to be colored manually with |CreateColorMask()|.
  gfx::ImageSkia google_icon =
      gfx::CreateVectorIcon(kGoogleGLogoIcon, icon_size - kIconPadding.width(),
                            gfx::kPlaceholderColor);
  gfx::ImageSkia grey_google_icon =
      gfx::ImageSkiaOperations::CreateColorMask(google_icon, icon_color);

  return gfx::CanvasImageSource::CreatePadded(grey_google_icon, kIconPadding);
}
#endif

// Returns the number of browsers associated with |profile|.
// Note: For regular profiles this includes incognito sessions.
int CountBrowsersFor(Profile* profile) {
  int browser_count = chrome::GetBrowserCount(profile);
  if (!profile->IsOffTheRecord() && profile->HasOffTheRecordProfile())
    browser_count += chrome::GetBrowserCount(profile->GetOffTheRecordProfile());
  return browser_count;
}

SkColor GetSyncErrorBackgroundColor(bool sync_paused) {
  constexpr int kAlpha = 16;
  ui::NativeTheme::ColorId base_color_id =
      sync_paused ? ui::NativeTheme::kColorId_ProminentButtonColor
                  : ui::NativeTheme::kColorId_AlertSeverityHigh;
  SkColor base_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(base_color_id);
  return SkColorSetA(base_color, kAlpha);
}

bool IsSyncPaused(Profile* profile) {
  int unused;
  return sync_ui_util::GetMessagesForAvatarSyncError(
             profile, &unused, &unused) == sync_ui_util::AUTH_ERROR;
}

}  // namespace

// ProfileMenuView ---------------------------------------------------------

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(views::Button* anchor_button,
                                       Browser* browser,
                                       signin_metrics::AccessPoint access_point)
    : ProfileMenuViewBase(anchor_button, browser),
      access_point_(access_point),
      dice_enabled_(AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser->profile())) {
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
  set_close_on_deactivate(close_on_deactivate_for_testing_);
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::BuildMenu() {
  avatar_menu_ = std::make_unique<AvatarMenu>(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      this, browser());
  avatar_menu_->RebuildMenu();

  if (!base::FeatureList::IsEnabled(features::kProfileMenuRevamp)) {
    if (dice_enabled_) {
      // Fetch DICE accounts. Note: This always includes the primary account if
      // it is set.
      dice_accounts_ =
          signin_ui_util::GetAccountsForDicePromos(browser()->profile());
    }
    AddProfileMenuView(avatar_menu_.get());
    return;
  }

  Profile* profile = browser()->profile();
  if (profile->IsRegularProfile()) {
    BuildIdentity();
    BuildSyncInfo();
    BuildAutofillButtons();
  } else if (profile->IsGuestSession()) {
    BuildGuestIdentity();
  } else {
    NOTREACHED();
  }

  BuildFeatureButtons();
  BuildProfileManagementHeading();
  BuildSelectableProfiles();
  BuildProfileManagementFeatureButtons();
}

void ProfileMenuView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  // TODO(crbug.com/993752): Remove AvatarMenu observer.
}

void ProfileMenuView::FocusButtonOnKeyboardOpen() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileMenuView::OnWidgetClosing(views::Widget* /*widget*/) {
  // Unsubscribe from everything early so that the updates do not reach the
  // bubble and change its state.
  avatar_menu_.reset();
}

base::string16 ProfileMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  RecordClick(ActionableItem::kManageGoogleAccountButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_ManageGoogleAccountClicked"));

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasUnconsentedPrimaryAccount());

  NavigateToGoogleAccountPage(
      profile, identity_manager->GetUnconsentedPrimaryAccountInfo().email);
}

void ProfileMenuView::OnPasswordsButtonClicked() {
  RecordClick(ActionableItem::kPasswordsButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_PasswordsClicked"));
  NavigateToManagePasswordsPage(
      browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
}

void ProfileMenuView::OnCreditCardsButtonClicked() {
  RecordClick(ActionableItem::kCreditCardsButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_PaymentsClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
}

void ProfileMenuView::OnAddressesButtonClicked() {
  RecordClick(ActionableItem::kAddressesButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_AddressesClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
}

void ProfileMenuView::OnGuestProfileButtonClicked() {
  RecordClick(ActionableItem::kGuestProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_GuestClicked"));
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  RecordClick(ActionableItem::kManageProfilesButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_ManageClicked"));
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER);
}

void ProfileMenuView::OnLockButtonClicked() {
  RecordClick(ActionableItem::kLockButton);
  profiles::LockProfile(browser()->profile());
  // TODO(crbug.com/995757): Remove.
  PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
}

void ProfileMenuView::OnExitProfileButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_CloseAllClicked"));
  profiles::CloseProfileWindows(browser()->profile());
}

void ProfileMenuView::OnSyncSettingsButtonClicked() {
  RecordClick(ActionableItem::kSyncSettingsButton);
  chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
}

void ProfileMenuView::OnSyncErrorButtonClicked(
    sync_ui_util::AvatarSyncErrorType error) {
  RecordClick(ActionableItem::kSyncErrorButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_SignInAgainClicked"));
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
            profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(), access_point_);
      }
      break;
    case sync_ui_util::AUTH_ERROR:
      Hide();
      browser()->signin_view_controller()->ShowSignin(
          profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH, browser(), access_point_);
      break;
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      chrome::OpenUpdateChromeDialog(browser());
      break;
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_ERROR:
      sync_ui_util::OpenTabForSyncKeyRetrieval(browser());
      break;
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
    case sync_ui_util::NO_SYNC_ERROR:
      NOTREACHED();
      break;
  }
}

void ProfileMenuView::OnCurrentProfileCardClicked() {
  RecordClick(ActionableItem::kCurrentProfileCard);
  if (dice_enabled_ &&
      IdentityManagerFactory::GetForProfile(browser()->profile())
          ->HasPrimaryAccount()) {
    chrome::ShowSettingsSubPage(browser(), chrome::kPeopleSubPage);
  } else {
    // Open settings to edit profile name and image. The profile doesn't need
    // to be authenticated to open this.
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
  }
}

void ProfileMenuView::OnSigninButtonClicked() {
  RecordClick(ActionableItem::kSigninButton);
  Hide();
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(), access_point_);
}

void ProfileMenuView::OnSigninAccountButtonClicked(AccountInfo account) {
  RecordClick(ActionableItem::kSigninAccountButton);
  Hide();
  signin_ui_util::EnableSyncFromPromo(browser(), account, access_point_,
                                      true /* is_default_promo_account */);
}

void ProfileMenuView::OnSignoutButtonClicked() {
  RecordClick(ActionableItem::kSignoutButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("Signin_Signout_FromUserMenu"));
  Hide();
  // Sign out from all accounts.
  IdentityManagerFactory::GetForProfile(browser()->profile())
      ->GetAccountsMutator()
      ->RemoveAllAccounts(signin_metrics::SourceForRefreshTokenOperation::
                              kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  RecordClick(ActionableItem::kOtherProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_ProfileClicked"));
  Hide();
  profiles::SwitchToProfile(profile_path, /*always_create=*/false,
                            ProfileManager::CreateCallback(),
                            ProfileMetrics::SWITCH_PROFILE_ICON);
}

void ProfileMenuView::OnCookiesClearedOnExitLinkClicked() {
  RecordClick(ActionableItem::kCookiesClearedOnExitLink);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_CookieSettingsClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
}

void ProfileMenuView::OnAddNewProfileButtonClicked() {
  RecordClick(ActionableItem::kAddNewProfileButton);
  UserManager::Show(/*profile_path_to_focus=*/base::FilePath(),
                    profiles::USER_MANAGER_OPEN_CREATE_USER_PAGE);
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  RecordClick(ActionableItem::kEditProfileButton);
  chrome::ShowSettingsSubPage(browser(), chrome::kManageProfileSubPage);
}

void ProfileMenuView::BuildIdentity() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetUnconsentedPrimaryAccountInfo();
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          account);
  ProfileAttributesEntry* profile_attributes =
      GetProfileAttributesEntry(profile);
  size_t num_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();

  if (num_of_profiles > 1 || !profile_attributes->IsUsingDefaultName()) {
    SetHeading(profile_attributes->GetLocalProfileName(),
               l10n_util::GetStringUTF16(IDS_SETTINGS_EDIT_PERSON),
               base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                                   base::Unretained(this)));
  }

  if (account_info.has_value()) {
    SetIdentityInfo(
        account_info.value().account_image.AsImageSkia(), GetSyncIcon(),
        base::UTF8ToUTF16(account_info.value().full_name),
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.value().email));
  } else {
    SetIdentityInfo(
        profile_attributes->GetAvatarIcon().AsImageSkia(), GetSyncIcon(),
        /*title=*/base::string16(),
        l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  SetIdentityInfo(profiles::GetGuestAvatar(), GetSyncIcon(),
                  l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME));
}

gfx::ImageSkia ProfileMenuView::GetSyncIcon() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  if (!profile->IsRegularProfile())
    return gfx::ImageSkia();

  if (!identity_manager->HasPrimaryAccount())
    return ColoredImageForMenu(kSyncPausedCircleIcon, gfx::kGoogleGrey500);

  const gfx::VectorIcon* icon = nullptr;
  ui::NativeTheme::ColorId color_id;
  int unused;
  switch (
      sync_ui_util::GetMessagesForAvatarSyncError(profile, &unused, &unused)) {
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
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      icon = &kSyncPausedCircleIcon;
      color_id = ui::NativeTheme::kColorId_AlertSeverityHigh;
      break;
  }
  return ColoredImageForMenu(*icon, native_theme->GetSystemColor(color_id));
}

void ProfileMenuView::BuildAutofillButtons() {
  AddShortcutFeatureButton(
      ImageForMenu(kKeyIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      ImageForMenu(kCreditCardIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      ImageForMenu(vector_icons::kLocationOnIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
  Profile* profile = browser()->profile();
  // Only show the sync info if signin and sync are allowed.
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      !profile->IsSyncAllowed()) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager->HasPrimaryAccount()) {
    // Show sync state.
    int description_string_id, button_string_id;
    sync_ui_util::AvatarSyncErrorType error =
        sync_ui_util::GetMessagesForAvatarSyncError(
            browser()->profile(), &description_string_id, &button_string_id);

    if (error == sync_ui_util::NO_SYNC_ERROR) {
      SetSyncInfo(
          GetSyncIcon(),
          /*description=*/base::string16(),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON),
          base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                              base::Unretained(this)));
    } else {
      bool sync_paused = (error == sync_ui_util::AUTH_ERROR);
      // Overwrite error description with short version for the menu.
      description_string_id = sync_paused ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
                                          : IDS_SYNC_ERROR_USER_MENU_TITLE;

      SetSyncInfo(
          GetSyncIcon(), l10n_util::GetStringUTF16(description_string_id),
          l10n_util::GetStringUTF16(button_string_id),
          base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                              base::Unretained(this), error));
      SetSyncInfoBackgroundColor(GetSyncErrorBackgroundColor(sync_paused));
    }
    return;
  }

  // Show sync promos.
  CoreAccountInfo unconsented_account =
      identity_manager->GetUnconsentedPrimaryAccountInfo();
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          unconsented_account);

  if (account_info.has_value()) {
    SetSyncInfo(
        GetSyncIcon(),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnSigninAccountButtonClicked,
                            base::Unretained(this), account_info.value()));
  } else {
    SetSyncInfo(/*icon=*/gfx::ImageSkia(),
                l10n_util::GetStringUTF16(
                    IDS_SETTINGS_SYNC_SIGN_IN_PROMPT_WITH_NO_ACCOUNT),
                l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
                base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                                    base::Unretained(this)));
  }

  SetSyncInfoBackgroundColor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor));
}

void ProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool is_guest = profile->IsGuestSession();
  const bool has_unconsented_account =
      !is_guest && identity_manager->HasUnconsentedPrimaryAccount();
  const bool has_primary_account =
      !is_guest && identity_manager->HasPrimaryAccount();

  if (has_unconsented_account && !IsSyncPaused(profile)) {
    AddFeatureButton(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        // The Google G icon needs to be shrunk, so it won't look too big
        // compared to the other icons.
        ImageForMenu(kGoogleGLogoIcon, /*icon_to_image_ratio=*/0.75),
#else
        gfx::ImageSkia(),
#endif
        l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
        base::BindRepeating(
            &ProfileMenuView::OnManageGoogleAccountButtonClicked,
            base::Unretained(this)));
  }

  int window_count = CountBrowsersFor(profile);
  if (window_count > 1) {
    AddFeatureButton(
        ImageForMenu(vector_icons::kCloseIcon),
        l10n_util::GetPluralStringFUTF16(IDS_PROFILES_CLOSE_X_WINDOWS_BUTTON,
                                         window_count),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)));
  }

  // The sign-out button is always at the bottom.
  if (has_unconsented_account && !has_primary_account) {
    AddFeatureButton(
        ImageForMenu(kSignOutIcon),
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
        base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementHeading() {
  SetProfileManagementHeading(
      l10n_util::GetStringUTF16(IDS_PROFILES_OTHER_PROFILES_TITLE));
}

void ProfileMenuView::BuildSelectableProfiles() {
  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributes();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath())
      continue;

    AddSelectableProfile(
        profile_entry->GetAvatarIcon().AsImageSkia(), profile_entry->GetName(),
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }

  if (!browser()->profile()->IsGuestSession()) {
    AddSelectableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME),
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  AddProfileManagementShortcutFeatureButton(
      ImageForMenu(vector_icons::kSettingsIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_USERS_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                          base::Unretained(this)));

  AddProfileManagementFeatureButton(
      ImageForMenu(kAddIcon, /*icon_to_image_ratio=*/0.75),
      l10n_util::GetStringUTF16(IDS_ADD),
      base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::AddProfileMenuView(AvatarMenu* avatar_menu) {
  // Separate items into active and alternatives.
  const AvatarMenu::Item* active_item = nullptr;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active) {
      active_item = &avatar_menu->GetItemAt(i);
      break;
    }
  }

  bool sync_error =
      active_item ? AddSyncErrorViewIfNeeded(*active_item) : false;

  if (!sync_error || !dice_enabled_) {
    // Guest windows don't have an active profile.
    if (active_item)
      AddCurrentProfileView(*active_item, /* is_guest = */ false);
    else
      AddGuestProfileView();
  }

  if (dice_enabled_ && !dice_accounts_.empty() &&
      !SigninErrorControllerFactory::GetForProfile(browser()->profile())
           ->HasError()) {
    AddManageGoogleAccountButton();
  }

  if (browser()->profile()->IsSupervised())
    AddSupervisedUserDisclaimerView();

  if (active_item)
    AddAutofillHomeView();

  const bool display_lock = active_item && active_item->signed_in &&
                            profiles::IsLockAvailable(browser()->profile());
  AddOptionsView(display_lock, avatar_menu);
}

bool ProfileMenuView::AddSyncErrorViewIfNeeded(
    const AvatarMenu::Item& avatar_item) {
  int content_string_id, button_string_id;
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser()->profile(), &content_string_id, &button_string_id);
  if (error == sync_ui_util::NO_SYNC_ERROR)
    return false;

  if (dice_enabled_) {
    AddDiceSyncErrorView(avatar_item, error, button_string_id);
  } else {
    AddPreDiceSyncErrorView(avatar_item, error, button_string_id,
                            content_string_id);
  }

  return true;
}

void ProfileMenuView::AddPreDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id,
    int content_string_id) {
  AddMenuGroup();
  auto sync_problem_icon = std::make_unique<views::ImageView>();
  sync_problem_icon->SetImage(
      gfx::CreateVectorIcon(kSyncProblemIcon, BadgedProfilePhoto::kImageSize,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_AlertSeverityHigh)));
  views::Button* button = CreateAndAddTitleCard(
      std::move(sync_problem_icon),
      l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE),
      l10n_util::GetStringUTF16(content_string_id), base::RepeatingClosure());
  static_cast<HoverButton*>(button)->SetStyle(HoverButton::STYLE_ERROR);

  // Adds an action button if an action exists.
  if (button_string_id) {
    CreateAndAddBlueButton(
        l10n_util::GetStringUTF16(button_string_id), true /* md_style */,
        base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                            base::Unretained(this), error));
  }
}

void ProfileMenuView::AddDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id) {
  // Creates a view containing an error hover button displaying the current
  // profile (only selectable when sync is paused or disabled) and when sync is
  // not disabled there is a blue button to resolve the error.
  const bool show_sync_paused_ui = error == sync_ui_util::AUTH_ERROR;
  const bool sync_disabled = !browser()->profile()->IsSyncAllowed();

  AddMenuGroup();

  if (show_sync_paused_ui &&
      base::FeatureList::IsEnabled(
          features::kShowSyncPausedReasonCookiesClearedOnExit) &&
      AreSigninCookiesClearedOnExit(browser()->profile())) {
    AddSyncPausedReasonCookiesClearedOnExit();
  }
  // Add profile card.
  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      show_sync_paused_ui
          ? BadgedProfilePhoto::BADGE_TYPE_SYNC_PAUSED
          : sync_disabled ? BadgedProfilePhoto::BADGE_TYPE_SYNC_DISABLED
                          : BadgedProfilePhoto::BADGE_TYPE_SYNC_ERROR,
      avatar_item.icon);
  views::Button* current_profile_card = CreateAndAddTitleCard(
      std::move(current_profile_photo),
      l10n_util::GetStringUTF16(
          show_sync_paused_ui
              ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
              : sync_disabled ? IDS_PROFILES_DICE_SYNC_DISABLED_TITLE
                              : IDS_SYNC_ERROR_USER_MENU_TITLE),
      avatar_item.username,
      base::BindRepeating(&ProfileMenuView::OnCurrentProfileCardClicked,
                          base::Unretained(this)));

  if (!show_sync_paused_ui && !sync_disabled) {
    static_cast<HoverButton*>(current_profile_card)
        ->SetStyle(HoverButton::STYLE_ERROR);
    current_profile_card->SetEnabled(false);
  }

  if (!sync_disabled) {
    CreateAndAddBlueButton(
        l10n_util::GetStringUTF16(button_string_id), true /* md_style */,
        base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                            base::Unretained(this), error));
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_SignInAgainDisplayed"));
  }
}

void ProfileMenuView::AddSyncPausedReasonCookiesClearedOnExit() {
  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_SYNC_PAUSED_REASON_CLEAR_COOKIES_ON_EXIT_LINK_TEXT);
  size_t link_begin = 0;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_SYNC_PAUSED_REASON_CLEAR_COOKIES_ON_EXIT, link_text, &link_begin);

  CreateAndAddLabelWithLink(
      text, gfx::Range(link_begin, link_begin + link_text.length()),
      base::BindRepeating(&ProfileMenuView::OnCookiesClearedOnExitLinkClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::AddCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  Profile* profile = browser()->profile();
  const bool sync_disabled = !profile->IsSyncAllowed();
  if (!is_guest && sync_disabled) {
    AddDiceSyncErrorView(avatar_item, sync_ui_util::NO_SYNC_ERROR, 0);
    return;
  }

  if (!avatar_item.signed_in && dice_enabled_ &&
      SyncPromoUI::ShouldShowSyncPromo(profile)) {
    AddDiceSigninView();
    return;
  }

  AddMenuGroup();

  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      GetProfileBadgeType(profile), avatar_item.icon);
  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile->GetPath());

  // Show the profile name by itself if not signed in or account consistency is
  // disabled. Otherwise, show the email attached to the profile.
  bool show_email = !is_guest && avatar_item.signed_in;
  const base::string16 hover_button_title =
      dice_enabled_ && profile->IsSyncAllowed() && show_email
          ? l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_COMPLETE_TITLE)
          : profile_name;

  views::Button* current_profile_card = CreateAndAddTitleCard(
      std::move(current_profile_photo), hover_button_title,
      show_email ? avatar_item.username : base::string16(),
      base::BindRepeating(&ProfileMenuView::OnCurrentProfileCardClicked,
                          base::Unretained(this)));
  // TODO(crbug.com/815047): Sometimes, |avatar_item.username| is empty when
  // |show_email| is true, which should never happen. This causes a crash when
  // setting the elision behavior, so until this bug is fixed, avoid the crash
  // by checking that the username is not empty.
  if (show_email && !avatar_item.username.empty())
    static_cast<HoverButton*>(current_profile_card)
        ->SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);

  // The available links depend on the type of profile that is active.
  if (is_guest) {
    current_profile_card->SetEnabled(false);
  } else if (avatar_item.signed_in) {
    current_profile_card->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME, profile_name,
        avatar_item.username));
  } else {
    bool is_signin_allowed =
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
    // For the dice promo equivalent, see AddDiceSigninPromo() call sites.
    if (!dice_enabled_ && is_signin_allowed)
      AddPreDiceSigninPromo();

    current_profile_card->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME, profile_name));
  }
}

void ProfileMenuView::AddPreDiceSigninPromo() {
  AddMenuGroup(false /* add_separator */);
  CreateAndAddLabel(l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));

  CreateAndAddBlueButton(
      l10n_util::GetStringFUTF16(
          IDS_SYNC_START_SYNC_BUTTON_LABEL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
      true /* md_style */,
      base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                          base::Unretained(this)));

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

void ProfileMenuView::AddDiceSigninPromo() {
  AddMenuGroup();

  // Show promo illustration + text when there is no promo account.
  if (GetDiceSigninPromoShowCount() <=
      kDiceSigninPromoIllustrationShowCountMax) {
    // Add the illustration.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    std::unique_ptr<NonAccessibleImageView> illustration =
        std::make_unique<NonAccessibleImageView>();
    illustration->SetImage(
        *rb.GetNativeImageNamed(IDR_PROFILES_DICE_TURN_ON_SYNC).ToImageSkia());
    AddViewItem(std::move(illustration));
  }
  // Add the promo text.
  CreateAndAddLabel(l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO));

  // Create a sign-in button without account information.
  CreateAndAddDiceSigninButton(
      /*account_info=*/nullptr, /*account_icon=*/nullptr,
      base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::AddDiceSigninView() {
  IncrementDiceSigninPromoShowCount();
  // Create a view that holds an illustration, a promo text and a button to turn
  // on Sync. The promo illustration is only shown the first 10 times per
  // profile.
  const bool promo_account_available = !dice_accounts_.empty();

  // Log sign-in impressions user metrics.
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      promo_account_available);

  if (!promo_account_available) {
    // For the pre-dice promo equivalent, see AddPreDiceSigninPromo() call
    // sites.
    AddDiceSigninPromo();
    return;
  }

  AddMenuGroup();
  // Create a button to sign in the first account of |dice_accounts_|.
  AccountInfo dice_promo_default_account = dice_accounts_[0];
  gfx::Image account_icon = dice_promo_default_account.account_image;
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  CreateAndAddDiceSigninButton(
      &dice_promo_default_account, &account_icon,
      base::BindRepeating(&ProfileMenuView::OnSigninAccountButtonClicked,
                          base::Unretained(this), dice_promo_default_account));

  // Add sign out button.
  CreateAndAddBlueButton(
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT), false /* md_style */,
      base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::AddGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guest_avatar_item(0, base::FilePath(), guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  AddCurrentProfileView(guest_avatar_item, true);
}

void ProfileMenuView::AddOptionsView(bool display_lock,
                                        AvatarMenu* avatar_menu) {
  AddMenuGroup();

  const bool is_guest = browser()->profile()->IsGuestSession();
  // Add the user switching buttons.
  // Order them such that the active user profile comes first (for Dice).
  std::vector<size_t> ordered_item_indices;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active)
      ordered_item_indices.insert(ordered_item_indices.begin(), i);
    else
      ordered_item_indices.push_back(i);
  }
  for (size_t profile_index : ordered_item_indices) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(profile_index);
    if (!item.active) {
      gfx::Image image = profiles::GetSizedAvatarIcon(
          item.icon, true, GetDefaultIconSize(), GetDefaultIconSize(),
          profiles::SHAPE_CIRCLE);
      views::Button* button = CreateAndAddButton(
          *image.ToImageSkia(), profiles::GetProfileSwitcherTextForItem(item),
          base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                              base::Unretained(this), item.profile_path));

      if (!first_profile_button_)
        first_profile_button_ = button;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        first_profile_button_);

  // Add the "Guest" button for browsing as guest
  if (!is_guest && !browser()->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      CreateAndAddButton(
          CreateVectorIcon(kUserMenuGuestIcon),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_GUEST_PROFILE_BUTTON),
          base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                              base::Unretained(this)));
    }
  }

  if (is_guest) {
    CreateAndAddButton(
        CreateVectorIcon(kCloseAllIcon),
        l10n_util::GetStringUTF16(IDS_PROFILES_EXIT_GUEST),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)));
  } else {
    CreateAndAddButton(
        CreateVectorIcon(vector_icons::kSettingsIcon),
        l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_USERS_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                            base::Unretained(this)));
  }

  if (display_lock) {
    lock_button_ = CreateAndAddButton(
        gfx::CreateVectorIcon(vector_icons::kLockIcon, GetDefaultIconSize(),
                              gfx::kChromeIconGrey),
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnLockButtonClicked,
                            base::Unretained(this)));
  } else if (!is_guest) {
    AvatarMenu::Item active_avatar_item =
        avatar_menu->GetItemAt(ordered_item_indices[0]);
    CreateAndAddButton(
        CreateVectorIcon(kCloseAllIcon),
        avatar_menu->GetNumberOfItems() >= 2
            ? l10n_util::GetStringFUTF16(IDS_PROFILES_EXIT_PROFILE_BUTTON,
                                         active_avatar_item.name)
            : l10n_util::GetStringUTF16(IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::AddSupervisedUserDisclaimerView() {
  AddMenuGroup();
  auto* disclaimer = CreateAndAddLabel(
      avatar_menu_->GetSupervisedUserInformation(), CONTEXT_BODY_TEXT_SMALL);
  disclaimer->SetAllowCharacterBreak(true);
}

void ProfileMenuView::AddAutofillHomeView() {
  if (browser()->profile()->IsGuestSession())
    return;

  AddMenuGroup();

  // Passwords.
  CreateAndAddButton(
      CreateVectorIcon(kKeyIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  // Credit cards.
  CreateAndAddButton(
      CreateVectorIcon(kCreditCardIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  // Addresses.
  CreateAndAddButton(
      CreateVectorIcon(vector_icons::kLocationOnIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::AddManageGoogleAccountButton() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddMenuGroup(false);
  CreateAndAddButton(
      GetGoogleIconForUserMenu(GetDefaultIconSize()),
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
      base::BindRepeating(&ProfileMenuView::OnManageGoogleAccountButtonClicked,
                          base::Unretained(this)));
#endif
}

void ProfileMenuView::PostActionPerformed(
    ProfileMetrics::ProfileDesktopMenu action_performed) {
  ProfileMetrics::LogProfileDesktopMenu(action_performed);
}

int ProfileMenuView::GetDiceSigninPromoShowCount() const {
  return browser()->profile()->GetPrefs()->GetInteger(
      prefs::kDiceSigninUserMenuPromoCount);
}

void ProfileMenuView::IncrementDiceSigninPromoShowCount() {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, GetDiceSigninPromoShowCount() + 1);
}
