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
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_hats_util.h"
#include "chrome/browser/signin/signin_promo_util.h"
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
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/features.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace {

std::u16string GetSyncErrorButtonText(
    Profile* profile,
    syncer::SyncService::UserActionableError error) {
  if (error == syncer::SyncService::UserActionableError::kUnrecoverableError &&
      !ChromeSigninClientFactory::GetForProfile(profile)
           ->IsClearPrimaryAccountAllowed()) {
    // Only shown for "Sync-the-feature".
    return l10n_util::GetStringUTF16(
        IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON_MAYBE_TITLE_CASE);
  }

  if (error == syncer::SyncService::UserActionableError::kSignInNeedsUpdate &&
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSync)) {
    // Only shown for "Sync-the-feature".
    return l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_BUTTON_MAYBE_TITLE_CASE);
  }

  return l10n_util::GetStringUTF16(
      GetSyncErrorButtonStringId(error, /*support_title_case=*/true));
}

std::u16string GetProfileIdentifier(const ProfileAttributesEntry& entry) {
  switch (entry.GetNameForm()) {
    case NameForm::kGaiaName:
    case NameForm::kLocalName:
      return entry.GetName();
    case NameForm::kGaiaAndLocalName:
      return l10n_util::GetStringFUTF16(
          IDS_PROFILE_MENU_PROFILE_IDENTIFIER_WITH_SEPARATOR,
          entry.GetGAIANameToDisplay(), entry.GetLocalProfileName());
  }
}

}  // namespace

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(
    ui::TrackedElement* anchor_element,
    Browser* browser,
    signin::ProfileMenuAvatarButtonPromoInfo promo_info,
    bool from_avatar_promo)
    : ProfileMenuViewBase(anchor_element, browser),
      browser_(raw_ref<Browser>::from_ptr(browser)),
      promo_info_(promo_info),
      from_avatar_promo_(from_avatar_promo) {
  set_close_on_deactivate(close_on_deactivate_for_testing_);

  // Set the callback to launch a HaTS survey upon menu dismissal.
  // We use `SetCloseCallback` instead of `SetCancelCallback` because the close
  // callback is also executed when users dismiss the menu by clicking outside,
  // unlike the cancel callback.
  SetCloseCallback(
      base::BindOnce(&ProfileMenuView::OnClose, base::Unretained(this)));
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::OnClose() {
  bool is_browser_window_active =
      skip_window_active_check_for_testing_ ||
      (browser().window() && browser().window()->IsActive());
  if (!actionable_item_clicked() && is_browser_window_active) {
    // Launch a HaTS survey only if the user dismissed the menu without
    // selecting an item (e.g., by clicking outside or pressing ESC), and the
    // browser window remains active.
    signin::LaunchSigninHatsSurveyForProfile(
        kHatsSurveyTriggerIdentityProfileMenuDismissed, &profile());
  }
}

void ProfileMenuView::BuildMenu() {
  if (profile().IsGuestSession()) {
    BuildGuestIdentity();
    MaybeBuildCloseBrowsersButton();
    AddBottomMargin();
    return;
  }

  CHECK(!profile().IsOffTheRecord());
  SetMenuTitleForAccessibility();
  BuildIdentityWithCallToAction();

  const bool is_web_app = web_app::AppBrowserController::IsWebApp(&browser());
  if (is_web_app) {
    BrowserUserEducationInterface::From(&browser())
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  // Users should not be able to use features from WebApps.
  if (!is_web_app) {
    BuildFeatureButtons();
  }

  std::vector<ProfileAttributesEntry*> available_profiles;
  GetProfilesForOtherProfilesSection(available_profiles);

  if (!available_profiles.empty()) {
    SetProfileManagementHeading(
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PROFILES_LIST_TITLE));
  }
  BuildOtherProfilesSection(available_profiles);
  base::UmaHistogramBoolean("ProfileChooser.HasProfilesShown",
                            !available_profiles.empty());

  // Users should not be able to manage profiles from WebApps.
  if (!is_web_app) {
    BuildProfileManagementFeatureButtons();
  }
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

void ProfileMenuView::OnProfileManagementButtonClicked() {
  OnActionableItemClicked(ActionableItem::kProfileManagementLabel);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ExecuteCommand(&browser(), IDC_SHOW_MANAGEMENT_PAGE);
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  OnActionableItemClicked(ActionableItem::kManageGoogleAccountButton);
  if (!perform_menu_actions()) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  NavigateToGoogleAccountPage(
      &profile(),
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

void ProfileMenuView::OnGuestProfileButtonClicked() {
  OnActionableItemClicked(ActionableItem::kGuestProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  DCHECK(profiles::IsGuestModeEnabled(profile()));
  profiles::SwitchToGuestProfile();
}

void ProfileMenuView::OnExitProfileButtonClicked() {
  OnActionableItemClicked(ActionableItem::kExitProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  profiles::CloseProfileWindows(&profile());
}

void ProfileMenuView::OnSyncSettingsButtonClicked() {
  OnActionableItemClicked(ActionableItem::kSyncSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(&browser(), chrome::kSyncSetupSubPage);
}

void ProfileMenuView::OnGoogleServicesSettingsButtonClicked() {
  OnActionableItemClicked(ActionableItem::kGoogleServicesSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(&browser(), chrome::kGoogleServicesSubpage);
}

void ProfileMenuView::OnAccountSettingsButtonClicked() {
  OnActionableItemClicked(ActionableItem::kAccountSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(&browser(), chrome::kPeopleSubPage);
}

void ProfileMenuView::OnPasskeyUnlockButtonClicked() {
  OnActionableItemClicked(ActionableItem::kPasskeyUnlockButton);
  if (!perform_menu_actions()) {
    return;
  }
  webauthn::PasskeyUnlockManager::OpenTabWithPasskeyUnlockChallenge(&browser());
}

void ProfileMenuView::OnSyncErrorButtonClicked(
    syncer::SyncService::UserActionableError error) {
  OnActionableItemClicked(ActionableItem::kSyncErrorButton);
  if (!perform_menu_actions()) {
    return;
  }

  // The logic below must be consistent with GetSyncInfoForAvatarErrorType().
  switch (error) {
    case syncer::SyncService::UserActionableError::kUnrecoverableError: {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(&profile());
      // Managed users get directed to the sign-out confirmation dialog in
      // settings.
      if (!ChromeSigninClientFactory::GetForProfile(&profile())
               ->IsClearPrimaryAccountAllowed()) {
        chrome::ShowSettingsSubPage(&browser(), chrome::kSignOutSubPage);
        break;
      }
      // This error means that the Sync engine failed to initialize. Shutdown
      // Sync engine by revoking sync consent.
      identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
          signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      // Re-enable sync with the same primary account.
      signin_ui_util::EnableSyncFromSingleAccountPromo(
          &profile(),
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin_metrics::AccessPoint::kAvatarBubbleSignIn);
      break;
    }
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          &profile(), signin_metrics::AccessPoint::kAvatarBubbleSignIn);
      break;
    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      chrome::OpenUpdateChromeDialog(&browser());
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      OpenTabForSyncKeyRetrieval(
          &browser(),
          trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      OpenTabForSyncKeyRecoverabilityDegraded(
          &browser(),
          trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      ShowSyncPassphraseDialogAndDecryptData(browser());
      break;
    case syncer::SyncService::UserActionableError::kNeedsSettingsConfirmation:
      chrome::ShowSettingsSubPage(&browser(), chrome::kSyncSetupSubPage);
      break;
    case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
      // TODO(crbug.com/452968646): Adjust this with providing the concrete
      // help center article link.
      break;
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
  }
}

void ProfileMenuView::OnSigninButtonClicked(
    CoreAccountInfo account,
    ActionableItem button_type,
    signin_metrics::AccessPoint access_point) {
  OnActionableItemClicked(button_type);

  if (!perform_menu_actions()) {
    return;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);

  // Pending while the HistorySyncPillExperiment is in progress.
  if (button_type == ActionableItem::kSigninReauthButton) {
    // The reauth button does not trigger a sync opt in.
    signin_ui_util::ShowReauthForAccount(&profile(), account.email,
                                         access_point);
    return;
  }
  signin_ui_util::EnableSyncFromSingleAccountPromo(&profile(), account,
                                                   access_point);
}

void ProfileMenuView::OnSignoutButtonClicked() {
  DCHECK(ChromeSigninClientFactory::GetForProfile(&profile())
             ->IsClearPrimaryAccountAllowed())
      << "Clear primary account is not allowed. Signout should not be offered "
         "in the UI.";

  OnActionableItemClicked(ActionableItem::kSignoutButton);
  if (!perform_menu_actions()) {
    return;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  browser().GetFeatures().signin_view_controller()->SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  OnActionableItemClicked(ActionableItem::kOtherProfileButton);
  if (!perform_menu_actions()) {
    return;
  }

  if (!web_app::AppBrowserController::IsWebApp(&browser())) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    // Switch to the selected profile and launch a HaTS survey for the
    // associated non-webapp browser.
    profiles::SwitchToProfile(
        profile_path, /*always_create=*/false,
        base::BindOnce([](Browser* browser) {
          if (!browser) {
            return;
          }
          signin::LaunchSigninHatsSurveyForProfile(
              kHatsSurveyTriggerIdentitySwitchProfileFromProfileMenu,
              browser->GetProfile());
        }));
  } else {
    // Open the same web app for another profile.
    // On non-macOS the only allowlisted case is PasswordManager WebApp, which
    // uses a different code path from other PWAs as it needs to not only
    // support switching profiles, but also possibly installing the app into a
    // different profile. Regular PWAs can only switch to profiles where the app
    // is already installed.
    const webapps::AppId& app_id = browser().app_controller()->app_id();
#if BUILDFLAG(IS_MAC)
    if (app_id != ash::kPasswordManagerAppId) {
      apps::AppShimManager::Get()->LaunchAppInProfile(app_id, profile_path);
      return;
    }
#endif
    CHECK_EQ(app_id, ash::kPasswordManagerAppId);

    app_profile_switcher_.emplace(
        app_id, profile(),
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
  OnActionableItemClicked(ActionableItem::kAddNewProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  OnActionableItemClicked(ActionableItem::kManageProfilesButton);
  if (!perform_menu_actions()) {
    return;
  }
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  OnActionableItemClicked(ActionableItem::kEditProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(&browser(), chrome::kManageProfileSubPage);
}

void ProfileMenuView::OnAutofillSettingsButtonClicked() {
  OnActionableItemClicked(ActionableItem::kAutofillSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  base::UmaHistogramEnumeration(
      "Autofill.AutofillAndPasswordsSettingsPage.VisitReferrer",
      autofill::autofill_metrics::AutofillSettingsReferrer::kProfileMenu);
  chrome::ShowSettingsSubPage(&browser(), chrome::kAutofillSubPage);
}

void ProfileMenuView::OnYourSavedInfoSettingsButtonClicked() {
  OnActionableItemClicked(ActionableItem::kAutofillSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.VisitReferrer",
      autofill::autofill_metrics::AutofillSettingsReferrer::kProfileMenu);
  chrome::ShowSettingsSubPage(&browser(), chrome::kYourSavedInfoSubPage);
}

void ProfileMenuView::OnBatchUploadButtonClicked(ActionableItem button_type) {
  OnActionableItemClicked(button_type);
  if (!perform_menu_actions()) {
    return;
  }

  BatchUploadService::EntryPoint batch_upload_entry_point;
  switch (button_type) {
    case ActionableItem::kBatchUploadButton:
      batch_upload_entry_point =
          BatchUploadService::EntryPoint::kProfileMenuRowButtonAction;
      break;
    case ActionableItem::kBatchUploadAsPrimaryButton:
      batch_upload_entry_point =
          from_avatar_promo_
              ? BatchUploadService::EntryPoint::
                    kProfileMenuPrimaryButtonActionFromAvatarPromo
              : BatchUploadService::EntryPoint::kProfileMenuPrimaryButtonAction;
      break;
    case ActionableItem::kBatchUploadWithBookmarksAsPrimaryButton:
      batch_upload_entry_point =
          from_avatar_promo_
              ? BatchUploadService::EntryPoint::
                    kProfileMenuPrimaryButtonWithBookmarksActionFromAvatarPromo
              : BatchUploadService::EntryPoint::
                    kProfileMenuPrimaryButtonWithBookmarksAction;
      break;
    case ActionableItem::kBatchUploadWindows10DepreciationAsPrimaryButton:
      batch_upload_entry_point =
          from_avatar_promo_
              ? BatchUploadService::EntryPoint::
                    kProfileMenuPrimaryButtonWithWindows10DepreciationActionFromAvatarPromo
              : BatchUploadService::EntryPoint::
                    kProfileMenuPrimaryButtonWithWindows10DepreciationAction;
      break;
    default:
      NOTREACHED() << "This actionable item should not trigger Batch Upload.";
  }

  BatchUploadServiceFactory::GetForProfile(&profile())
      ->OpenBatchUpload(&browser(), batch_upload_entry_point);
}

void ProfileMenuView::SetMenuTitleForAccessibility() {
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);

  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn: {
      std::string profile_user_display_name, profile_user_email;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
      profile_user_display_name = profile().GetPrefs()->GetString(
          enterprise_signin::prefs::kProfileUserDisplayName);
      profile_user_email = profile().GetPrefs()->GetString(
          enterprise_signin::prefs::kProfileUserEmail);
#endif
      menu_title_ =
          profile_user_display_name.empty()
              ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
              : base::UTF8ToUTF16(profile_user_display_name);
      menu_subtitle_ = base::UTF8ToUTF16(profile_user_email);  // Often empty.
    } break;
    case signin_util::SignedInState::kSyncPaused:
    case signin_util::SignedInState::kSignInPending:
      menu_title_ = base::UTF8ToUTF16(account_info.full_name);
      menu_subtitle_ =
          l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
      break;
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSignedIn:
      menu_title_ = base::UTF8ToUTF16(account_info.full_name);
      menu_subtitle_ = base::UTF8ToUTF16(account_info.email);
      break;
  }

  if (GetWidget()) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  menu_title_ = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  menu_subtitle_ = std::u16string();

  IdentitySectionParams params;
  params.title = menu_title_;
  params.profile_image_padding = std::nearbyint(kIdentityInfoImageSize * 0.25f);
  params.profile_image = profiles::GetGuestAvatar();
  SetProfileIdentityWithCallToAction(std::move(params));

  if (GetWidget()) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

ProfileMenuViewBase::IdentitySectionParams
ProfileMenuView::GetIdentitySectionParams(const ProfileAttributesEntry& entry) {
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  const CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo primary_extended_account_info =
      identity_manager->FindExtendedAccountInfo(primary_account_info);
  CoreAccountInfo account_info_for_signin_action = primary_account_info;

  const bool is_dasherless_profile = entry.IsDasherlessManagement();

  IdentitySectionParams params;
  params.title = GetProfileIdentifier(entry);
  profiles::PlaceholderAvatarIconParams icon_params = {.has_padding = true,
                                                       .has_background = false};
  params.profile_image = ui::ImageModel::FromImage(
      primary_extended_account_info.account_image.IsEmpty()
          ? entry.GetAvatarIcon(kIdentityInfoImageSize,
                                /*use_high_res_file=*/true, icon_params)
          : primary_extended_account_info.account_image);

  ui::ImageModel* custom_management_image = nullptr;
  if (enterprise_util::CanShowEnterpriseBadgingForMenu(&profile())) {
    if (profile().IsChild()) {
      params.header_string = l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    } else {
      params.header_string =
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PROFILE_MANAGED_HEADER);
      custom_management_image =
          policy::ManagementServiceFactory::GetForProfile(&profile())
              ->GetManagementIconForProfile();
    }

    params.header_action =
        base::BindRepeating(&ProfileMenuView::OnProfileManagementButtonClicked,
                            base::Unretained(this));
    if (custom_management_image) {
      params.header_image = *custom_management_image;
    } else {
      params.header_image = ui::ImageModel::FromVectorIcon(
          GetManagedUiIcon(&profile()), ui::kColorIcon);
    }
  }

  // Clarify Dasherless profile with subtitle while not adding the button.
  if (is_dasherless_profile) {
    params.subtitle =
        l10n_util::GetStringUTF16(IDS_PROFILES_DASHER_FEATURE_DISABLED_TITLE);
    return params;
  }

  if (web_app::AppBrowserController::IsWebApp(&browser())) {
    if (!primary_account_info.email.empty()) {
      params.subtitle = base::UTF8ToUTF16(primary_account_info.email);
    }
    return params;
  }

  syncer::SyncService* service = SyncServiceFactory::GetForProfile(&profile());
  syncer::SyncService::UserActionableError error =
      syncer::SyncService::UserActionableError::kNone;
  if (service) {
    error = service->GetUserActionableError();
    // Avoid reacting to
    // syncer::SyncService::UserActionableError::kSignInNeedsUpdate in case of
    // no sync consent, as kSignInPending is handled differently below.
    if (error != syncer::SyncService::UserActionableError::kNone &&
        (error !=
             syncer::SyncService::UserActionableError::kSignInNeedsUpdate ||
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync))) {
      params.subtitle =
          GetAvatarSyncErrorDescription(error, primary_account_info.email);
      params.button_text = GetSyncErrorButtonText(&profile(), error);
      params.button_action =
          base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                              base::Unretained(this), error);
      params.has_dotted_ring = true;
      return params;
    }
  }

  // If there are no sync user actionable errors, we can display passkey unlock
  // card if needed:
  if (error == syncer::SyncService::UserActionableError::kNone &&
      webauthn::PasskeyUnlockManager::IsPasskeyUnlockErrorUiEnabled()) {
    webauthn::PasskeyUnlockManager* passkey_unlock_manager =
        webauthn::PasskeyUnlockManagerFactory::GetForProfile(&profile());
    if (passkey_unlock_manager->ShouldDisplayErrorUi()) {
      params.subtitle =
          passkey_unlock_manager->GetPasskeyErrorProfileMenuDetails();
      params.button_text =
          passkey_unlock_manager->GetPasskeyErrorProfileMenuButtonLabel();
      params.button_action =
          base::BindRepeating(&ProfileMenuView::OnPasskeyUnlockButtonClicked,
                              base::Unretained(this));
      params.has_dotted_ring = true;
      return params;
    }
  }

  ActionableItem button_type = ActionableItem::kSigninAccountButton;
  signin_metrics::AccessPoint access_point =
      from_avatar_promo_ ? signin::kHistoryOptinAvatarPromoAccessPoint
                         : signin_metrics::AccessPoint::kAvatarBubbleSignIn;
  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn: {
      AccountInfo account_info_for_promos =
          signin_ui_util::GetSingleAccountForPromos(identity_manager);
      if (!CanOfferSignin(&profile(), account_info_for_promos.gaia,
                          account_info_for_promos.email,
                          /*allow_account_from_other_profile=*/true)
               .IsOk()) {
        break;
      }

      access_point =
          signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
      signin_metrics::LogSignInOffered(
          access_point,
          account_info_for_promos.IsEmpty()
              ? signin_metrics::PromoAction::
                    PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
              : signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);

      if (account_info_for_promos.IsEmpty()) {
        // Non-personalized signin button.
        button_type = ActionableItem::kSigninButton;
        params.subtitle = l10n_util::GetStringUTF16(
            base::FeatureList::IsEnabled(
                syncer::kReplaceSyncPromosWithSignInPromos)
                ? IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION_WITH_BOOKMARKS
                : IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION);
        params.button_text =
            l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON);
        break;
      }
      // "Continue as" signin button.
      account_info_for_signin_action = account_info_for_promos;
      params.subtitle = l10n_util::GetStringFUTF16(
          base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)
              ? IDS_SETTINGS_PEOPLE_ACCOUNT_AWARE_SIGNIN_ACCOUNT_ROW_SUBTITLE_WITH_EMAIL_WITH_BOOKMARKS
              : IDS_SETTINGS_PEOPLE_ACCOUNT_AWARE_SIGNIN_ACCOUNT_ROW_SUBTITLE_WITH_EMAIL,
          base::UTF8ToUTF16(account_info_for_promos.email));
      params.button_text = l10n_util::GetStringFUTF16(
          IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
          base::UTF8ToUTF16(!account_info_for_promos.given_name.empty()
                                ? account_info_for_promos.given_name
                                : account_info_for_promos.email));
      gfx::Image account_image;
      if (account_info_for_promos.account_image.IsEmpty()) {
        // No account image, use a placeholder.
        ProfileAttributesEntry* profile_attributes =
            g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile().GetPath());
        account_image = profile_attributes->GetAvatarIcon(
            /*size_for_placeholder_avatar=*/kIdentityImageSizeForButton,
            /*use_high_res_file=*/true,
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                browser().window()->GetColorProvider()->GetColor(
                    ui::kColorButtonBackgroundProminent)));
      } else {
        account_image = account_info_for_promos.account_image;
      }
      params.button_image =
          ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
              account_image,
              /*width=*/kIdentityImageSizeForButton,
              /*height=*/kIdentityImageSizeForButton, profiles::SHAPE_CIRCLE));
      break;
    }
    case signin_util::SignedInState::kSignedIn:
      if (promo_info_.type.has_value()) {
        switch (promo_info_.type.value()) {
          case signin::ProfileMenuAvatarButtonPromoInfo::Type::
              kHistorySyncPromo:
            params.subtitle = l10n_util::GetStringFUTF16(
                IDS_PROFILE_MENU_SYNC_PROMO_SYNC_HISTORY_DESCRIPTION,
                base::UTF8ToUTF16(primary_account_info.email));
            params.button_text = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_SYNC_PROMO_BUTTON_LABEL);
            button_type = ActionableItem::kHistorySyncButton;
            signin_metrics::LogHistorySyncOptInOffered(access_point);
            break;
          case signin::ProfileMenuAvatarButtonPromoInfo::Type::
              kBatchUploadPromo:
            params.email_subtitle =
                base::UTF8ToUTF16(primary_account_info.email);
            params.subtitle = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PROMO_DESCRIPTION_WITH_BATCH_UPLOAD);
            params.button_text = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PROMO_BUTTON_WITH_BATCH_UPLOAD);
            params.button_action = base::BindRepeating(
                &ProfileMenuView::OnBatchUploadButtonClicked,
                base::Unretained(this),
                ActionableItem::kBatchUploadAsPrimaryButton);
            break;
          case signin::ProfileMenuAvatarButtonPromoInfo::Type::
              kBatchUploadBookmarksPromo:
            params.email_subtitle =
                base::UTF8ToUTF16(primary_account_info.email);
            params.subtitle = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PROMO_DESCRIPTION_WITH_BATCH_UPLOAD_BOOKMARK_CLEANUP);
            params.button_text = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PROMO_BUTTON_WITH_BATCH_UPLOAD);
            params.button_action = base::BindRepeating(
                &ProfileMenuView::OnBatchUploadButtonClicked,
                base::Unretained(this),
                ActionableItem::kBatchUploadWithBookmarksAsPrimaryButton);
            break;
          case signin::ProfileMenuAvatarButtonPromoInfo::Type::
              kBatchUploadWindows10DepreciationPromo:
            params.email_subtitle =
                base::UTF8ToUTF16(primary_account_info.email);
            // Note: Sync promo does not explicitly mention "sync" but invites
            // the user to back-up their data. It is fine to be used here.
            params.subtitle = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_DESCRIPTION_WITH_SYNC_PROMO);
            params.button_text = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_BUTTON_LABEL_WITH_SYNC_PROMO);
            params.button_action = base::BindRepeating(
                &ProfileMenuView::OnBatchUploadButtonClicked,
                base::Unretained(this),
                ActionableItem::
                    kBatchUploadWindows10DepreciationAsPrimaryButton);
            break;
          case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
            CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
            params.subtitle = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_DESCRIPTION_WITH_SYNC_PROMO);
            params.button_text = l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_BUTTON_LABEL_WITH_SYNC_PROMO);
            break;
        }
      } else {
        if (base::FeatureList::IsEnabled(
                syncer::kReplaceSyncPromosWithSignInPromos)) {
          // No button.
          params.email_subtitle = base::UTF8ToUTF16(primary_account_info.email);
        } else {
          params.subtitle =
              l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO);
          params.button_text =
              l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
          signin_metrics::LogSyncOptInOffered(access_point);
        }
      }
      break;
    case signin_util::SignedInState::kSyncing:
      // No button.
      params.email_subtitle = base::UTF8ToUTF16(primary_account_info.email);
      break;
    case signin_util::SignedInState::kSignInPending:
      button_type = ActionableItem::kSigninReauthButton;
      params.subtitle = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PENDING_STATE_DESCRIPTION,
          base::UTF8ToUTF16(primary_account_info.email));
      params.button_text = l10n_util::GetStringUTF16(GetSyncErrorButtonStringId(
          syncer::SyncService::UserActionableError::kSignInNeedsUpdate,
          /*support_title_case=*/true));
      params.has_dotted_ring = true;
      signin_metrics::LogSigninPendingOffered(access_point);
      break;
    case signin_util::SignedInState::kSyncPaused:
      // Sync paused is covered by the sync errors path.
      NOTREACHED();
  }

  // Sets the default action if needed - if a button text was explicitly set and
  // no prior action was set.
  if (!params.button_text.empty() && params.button_action.is_null()) {
    params.button_action = base::BindRepeating(
        &ProfileMenuView::OnSigninButtonClicked, base::Unretained(this),
        account_info_for_signin_action, button_type, access_point);
  }

  return params;
}

void ProfileMenuView::BuildIdentityWithCallToAction() {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile().GetPath());
  if (!entry) {
    // May happen if the profile is being deleted. https://crbug.com/1040079
    return;
  }

  SetProfileIdentityWithCallToAction(GetIdentitySectionParams(*entry));
}

void ProfileMenuView::MaybeBuildBatchUploadButton() {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return;
  }

  if (promo_info_.local_data_count == 0) {
    return;
  }

  AddFeatureButton(
      l10n_util::GetPluralStringFUTF16(IDS_PROFILE_MENU_BATCH_UPLOAD_BUTTON,
                                       promo_info_.local_data_count),
      base::BindRepeating(&ProfileMenuView::OnBatchUploadButtonClicked,
                          base::Unretained(this),
                          ActionableItem::kBatchUploadButton),
      vector_icons::kSaveCloudIcon);
}

void ProfileMenuView::BuildAutofillSettingsButton() {
  CHECK(!profile().IsGuestSession());

  bool use_your_saved_info_branding =
      base::FeatureList::IsEnabled(
          autofill::features::kYourSavedInfoSettingsPage) ||
      base::FeatureList::IsEnabled(
          autofill::features::kYourSavedInfoBrandingInSettings);
  int message_id = use_your_saved_info_branding
                       ? IDS_SETTINGS_YOUR_SAVED_INFO
                       : IDS_PROFILE_MENU_AUTOFILL_SETTINGS_BUTTON;
  const gfx::VectorIcon& icon = use_your_saved_info_branding
                                    ? vector_icons::kPersonTextIcon
                                    : vector_icons::kPasswordManagerIcon;
  auto action = base::FeatureList::IsEnabled(
                    autofill::features::kYourSavedInfoSettingsPage)
                    ? &ProfileMenuView::OnYourSavedInfoSettingsButtonClicked
                    : &ProfileMenuView::OnAutofillSettingsButtonClicked;

  AddFeatureButton(l10n_util::GetStringUTF16(message_id),
                   base::BindRepeating(action, base::Unretained(this)), icon);
}

void ProfileMenuView::BuildCustomizeProfileButton() {
  CHECK(!profile().IsGuestSession());
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_CUSTOMIZE_PROFILE_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                          base::Unretained(this)),
      vector_icons::kEditChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildChromeAccountSettingsButton() {
  CHECK(!profile().IsGuestSession());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());

  if (!identity_manager) {
    return;
  }

  // Show the settings button only when signed in to Chrome or pending sign in.
  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignInPending:
    case signin_util::SignedInState::kSignedIn:
      break;
    case signin_util::SignedInState::kSyncPaused:
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn:
    case signin_util::SignedInState::kSyncing:
      return;
  }

  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_ACCOUNT_SETTINGS_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnAccountSettingsButtonClicked,
                          base::Unretained(this)),
      vector_icons::kSettingsChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildChromeAccountSettingsButtonWithSync() {
  CHECK(!profile().IsGuestSession());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());

  if (!identity_manager) {
    return;
  }

  // Show the settings button when signed in to Chrome or to the web, or if
  // signin is disallowed.
  const bool should_show_settings_button =
      !identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken()
           .empty() ||
      !profile().GetPrefs()->GetBoolean(prefs::kSigninAllowed);
  if (!should_show_settings_button) {
    return;
  }

  int message_id = IDS_PROFILE_MENU_OPEN_ACCOUNT_SETTINGS;
  const gfx::VectorIcon* icon = &vector_icons::kSettingsChromeRefreshIcon;
  if (signin_util::GetSignedInState(identity_manager) ==
      signin_util::SignedInState::kSyncing) {
    // Indicates clearly that Sync is ON.
    message_id = IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON;
    icon = &kSyncChromeRefreshIcon;
  }

  AddFeatureButton(
      l10n_util::GetStringUTF16(message_id),
      base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                          base::Unretained(this)),
      *icon);
}

void ProfileMenuView::MaybeBuildGoogleServicesSettingsButton() {
  CHECK(!profile().IsGuestSession());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());

  if (!identity_manager) {
    return;
  }

  // Show the services settings button  if signin is disallowed.
  if (profile().GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return;
  }
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_OPEN_ACCOUNT_SETTINGS),
      base::BindRepeating(
          &ProfileMenuView::OnGoogleServicesSettingsButtonClicked,
          base::Unretained(this)),
      vector_icons::kSettingsChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildManageGoogleAccountButton() {
  CHECK(!profile().IsGuestSession());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  if (!identity_manager) {
    return;
  }

  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignInPending:
    case signin_util::SignedInState::kSyncPaused:
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn:
      // Do not show the button.
      return;
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
      // Show the button.
      break;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The Google G icon needs to be shrunk, so it won't look too big compared
  // to the other icons.
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
      base::BindRepeating(&ProfileMenuView::OnManageGoogleAccountButtonClicked,
                          base::Unretained(this)),
      vector_icons::kGoogleGLogoIcon,
      /*icon_to_image_ratio=*/0.75f);
#else
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
      base::BindRepeating(&ProfileMenuView::OnManageGoogleAccountButtonClicked,
                          base::Unretained(this)),
      vector_icons::kFilterIcon);
#endif
}

void ProfileMenuView::MaybeBuildCloseBrowsersButton() {
  const int window_count = CountBrowsersFor(&profile());
  base::RepeatingClosure callback = base::BindRepeating(
      &ProfileMenuView::OnExitProfileButtonClicked, base::Unretained(this));
  int button_title_id = IDS_PROFILE_MENU_CLOSE_PROFILE_X_WINDOWS_BUTTON;

  if (profile().IsGuestSession()) {
    button_title_id = IDS_GUEST_PROFILE_MENU_CLOSE_X_WINDOWS_BUTTON;
  } else {
    // Show the button only if the current profile has multiple windows open.
    if (window_count <= 1) {
      return;
    }

    // And there are multiple profiles open.
    std::vector<Profile*> loaded_profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    Profile* profile_ptr = &profile();
    bool other_profile_open =
        std::any_of(loaded_profiles.begin(), loaded_profiles.end(),
                    [profile_ptr](Profile* loaded_profile) {
                      if (loaded_profile == profile_ptr) {
                        return false;
                      }
                      return CountBrowsersFor(loaded_profile) > 0;
                    });
    if (!other_profile_open) {
      return;
    }
  }

  AddFeatureButton(
      l10n_util::GetPluralStringFUTF16(button_title_id, window_count),
      std::move(callback), vector_icons::kCloseChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildSignoutButton() {
  CHECK(!profile().IsGuestSession());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  if (!identity_manager) {
    return;
  }

  const bool hide_signout_button_for_managed_profiles =
      enterprise_util::UserAcceptedAccountManagement(&profile());

  const bool add_sign_out_button =
      HasUnconstentedProfile(&profile()) &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      !hide_signout_button_for_managed_profiles;
  if (!add_sign_out_button) {
    return;
  }

  std::u16string signout_button_text;
  // Note: Sign out button is only added if there is a signed profile with no
  // sync consent, so there is no need to check these conditions for the sign in
  // pending state.
  const bool signin_pending =
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  signout_button_text = l10n_util::GetStringUTF16(
      signin_pending ? IDS_PROFILE_MENU_SIGN_OUT_WHEN_SIGNIN_PENDING
                     : IDS_PROFILE_MENU_SIGN_OUT);
  AddFeatureButton(signout_button_text,
                   base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                                       base::Unretained(this)),
                   kLogoutIcon);
}

void ProfileMenuView::BuildFeatureButtons() {
  CHECK(!profile().IsGuestSession());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile());
  // May add the button asynchronously, order is not be guaranteed.
  MaybeBuildBatchUploadButton();
  BuildAutofillSettingsButton();
  MaybeBuildManageGoogleAccountButton();
  BuildCustomizeProfileButton();
  (base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos) &&
   (!identity_manager ||
    !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)))
      ? MaybeBuildChromeAccountSettingsButton()
      : MaybeBuildChromeAccountSettingsButtonWithSync();
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    MaybeBuildGoogleServicesSettingsButton();
  }
  MaybeBuildCloseBrowsersButton();
  MaybeBuildSignoutButton();
}

void ProfileMenuView::GetProfilesForOtherProfilesSection(
    std::vector<ProfileAttributesEntry*>& available_profiles) const {
  CHECK(!profile().IsGuestSession());
#if BUILDFLAG(IS_MAC)
  const bool is_regular_web_app =
      web_app::AppBrowserController::IsWebApp(&browser()) &&
      (browser().app_controller()->app_id() != ash::kPasswordManagerAppId);
  std::set<base::FilePath> available_profile_paths;
  if (is_regular_web_app) {
    available_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(
            browser().app_controller()->app_id());
  }
#endif

  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByNameWithCheck();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == profile().GetPath()) {
      continue;
    }
    if (profile_entry->IsOmitted()) {
      continue;
    }

#if BUILDFLAG(IS_MAC)
    if (is_regular_web_app &&
        !available_profile_paths.contains(profile_entry->GetPath())) {
      continue;
    }
#endif

    available_profiles.push_back(profile_entry);
  }
}

void ProfileMenuView::BuildOtherProfilesSection(
    const std::vector<ProfileAttributesEntry*>& available_profiles) {
  for (ProfileAttributesEntry* profile_entry : available_profiles) {
    AddAvailableProfile(
        ui::ImageModel::FromImage(profile_entry->GetAvatarIcon(
            kOtherProfileImageSize,
            /*use_high_res_file=*/true,
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                browser().window()->GetColorProvider()->GetColor(
                    ui::kColorMenuBackground)))),
        profile_entry->GetName(),
        /*is_guest=*/false,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  CHECK(!profile().IsGuestSession());

  AddProfileManagementFeaturesSeparator();

  if (profiles::IsProfileCreationAllowed()) {
    AddProfileManagementFeatureButton(
        kAccountAddChromeRefreshIcon,
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_ADD_PROFILE),
        base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                            base::Unretained(this)));
  }

  if (profiles::IsGuestModeEnabled(profile()) &&
      !web_app::AppBrowserController::IsWebApp(&browser())) {
    AddProfileManagementFeatureButton(
        kAccountBoxIcon,
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_OPEN_GUEST_PROFILE),
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }

  AddProfileManagementFeatureButton(
      kAccountManageChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_MANAGE_PROFILES),
      base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                          base::Unretained(this)));
}
