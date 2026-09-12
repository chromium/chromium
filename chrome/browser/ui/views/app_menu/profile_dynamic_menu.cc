// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/profile_dynamic_menu.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/text_elider.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "google_apis/gaia/gaia_id.h"
#endif

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
// Returns the header title displayed at the top of the sync section.
// - If sync is paused or there is no account, displays "Not signed in" / local
// state.
// - If sign-in is pending verification, displays the user's email address.
// - If fully signed in, displays "Signed in as <email>".
std::u16string GetSyncSectionTitle(Profile* profile,
                                   signin::IdentityManager* identity_manager) {
  const AccountInfo account = GetAccountInfoFromProfile(profile);

  if (IsSyncPaused(profile) || account.IsEmpty()) {
    return l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
  }

  if (signin_util::IsSigninPending(identity_manager)) {
    return base::UTF8ToUTF16(account.GetEmail());
  }

  return l10n_util::GetStringFUTF16(
      IDS_PROFILE_ROW_SIGNED_IN_MESSAGE_WITH_EMAIL,
      {base::UTF8ToUTF16(account.GetEmail())});
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

ProfileDynamicMenu::ProfileDynamicMenu(BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {
  CHECK(browser_window_interface_);
}

ProfileDynamicMenu::~ProfileDynamicMenu() = default;

// Populates the actions inside the profile dynamic submenu.
// Structured in three sections:
//  1. Sync & sign-in status header and actionable recovery items.
//  2. Primary actions (Manage Google Account, Customize Chrome, Close Profile).
//  3. Other Profiles on this machine, followed by creation/management footer
//  options.
void ProfileDynamicMenu::BuildProfileActions(actions::BaseAction* parent_item) {
  if (!parent_item) {
    return;
  }
  // Clear any existing children to prevent duplicating items on re-population.
  parent_item->ResetActionList();

  Profile* profile = browser_window_interface_->GetProfile();
  CHECK(profile);

  // 1. Sync / Sign-in section (only for regular profiles; omitted for guest,
  // incognito, and isolated mode).
  if (!profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
      !profile->IsEnterpriseIsolatedModeProfile()) {
    if (BuildSyncSection(parent_item, profile)) {
      parent_item->AddChild(ActionAppMenuManager::CreateDividerActionItem());
    }
  }

  // 2. Other Profiles (omitted for guest & incognito).
  BuildOtherProfiles(parent_item);
}

void ProfileDynamicMenu::BuildOtherProfiles(actions::BaseAction* parent_item) {
  if (!parent_item || !browser_window_interface_) {
    return;
  }
  Profile* profile = browser_window_interface_->GetProfile();
  if (!profile) {
    return;
  }
  const ui::ColorProvider* color_provider = GetColorProvider();
  BuildOtherProfilesSection(parent_item, profile, color_provider);
}

// Populates the top sync/sign-in section of the profile submenu.
// Returns true if any items were added to `parent_item`.
bool ProfileDynamicMenu::BuildSyncSection(actions::BaseAction* parent_item,
                                          Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  // Check if sign-in is allowed by policy or configuration.
  if (!CanOfferSignin(profile, GaiaId(), /*email=*/std::string(),
                      /*allow_account_from_other_profile=*/true)
           .IsOk()) {
    return false;
  }

  if (!SyncServiceFactory::IsSyncAllowed(profile)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Section header showing sync status message or user email (e.g. "Signed in
  // as user@gmail.com").
  parent_item->AddChild(ActionAppMenuManager::CreateHeaderActionItem(
      GetSyncSectionTitle(profile, identity_manager)));

  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);
  if (service) {
    // Check for actionable sync errors (e.g., paused sync, passphrase needed,
    // client upgrade).
    const syncer::SyncService::UserActionableError error =
        service->GetUserActionableError();
    if (error != syncer::SyncService::UserActionableError::kNone) {
      actions::ActionId action_id = 0;
      const gfx::VectorIcon* icon = nullptr;
      int button_string_id =
          GetSyncErrorButtonStringId(error, /*support_title_case=*/true);
      switch (error) {
        case syncer::SyncService::UserActionableError::kNone:
          NOTREACHED();
        case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
          // User needs to re-enter credentials because sync is paused or
          // authentication expired.
          action_id = kActionShowSigninWhenPaused;
          if (IsSyncPaused(profile)) {
            button_string_id = IDS_SYNC_RELOGIN_BUTTON_MAYBE_TITLE_CASE;
            icon = &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSyncDisabledIcon
                         : vector_icons::kSyncOffChromeRefreshOldIcon);
          } else {
            icon = &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kAccountCircleOffIcon
                         : vector_icons::kAccountCircleOffChromeRefreshOldIcon);
          }
          break;
        case syncer::SyncService::UserActionableError::
            kNeedsTrustedVaultKeyForPasswords:
        case syncer::SyncService::UserActionableError::
            kTrustedVaultRecoverabilityDegradedForPasswords:
        case syncer::SyncService::UserActionableError::
            kTrustedVaultRecoverabilityDegradedForEverything:
        case syncer::SyncService::UserActionableError::
            kNeedsTrustedVaultKeyForEverything:
          // Prompt user to verify encryption key or recover trusted vault
          // security keys.
          action_id = kActionShowSigninWhenPaused;
          icon = &(features::IsRoundedIconsEnabled()
                       ? vector_icons::kAccountCircleOffIcon
                       : vector_icons::kAccountCircleOffChromeRefreshOldIcon);
          break;
        case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
          // Sync requires a newer version of Chrome.
          action_id = kActionUpgradeDialog;
          icon = &(features::IsRoundedIconsEnabled()
                       ? vector_icons::kErrorIcon
                       : vector_icons::kErrorOutlineOldIcon);
          break;
        case syncer::SyncService::UserActionableError::kNeedsPassphrase:
          // Custom sync passphrase required to decrypt data.
          action_id = kActionShowSyncPassphraseDialog;
          icon = &(features::IsRoundedIconsEnabled()
                       ? vector_icons::kErrorIcon
                       : vector_icons::kErrorOutlineOldIcon);
          break;
        case syncer::SyncService::UserActionableError::
            kNeedsSettingsConfirmation:
        case syncer::SyncService::UserActionableError::kUnrecoverableError:
          // Critical error requiring navigation to sync settings.
          action_id = kActionShowSyncSettings;
          icon = &(features::IsRoundedIconsEnabled()
                       ? vector_icons::kErrorIcon
                       : vector_icons::kErrorOutlineOldIcon);
          break;
        case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
          return true;
      }
      CHECK_NE(action_id, 0);
      CHECK(icon);
      parent_item->AddChild(ActionAppMenuManager::CreateIndirectActionItem(
          action_id, ActionAppMenuManager::DisplayType::kRow,
          ui::kColorMenuBackground, l10n_util::GetStringUTF16(button_string_id),
          ui::ImageModel::FromVectorIcon(
              *icon, ui::kColorMenuIcon,
              ui::SimpleMenuModel::kDefaultIconSize)));
      return true;
    }
  }

  // If there is no sync error, show either "Sync is on", a Sign-in promo, or
  // "Turn on sync".
  if (signin_util::GetSignedInState(identity_manager) ==
      signin_util::SignedInState::kSyncing) {
    // Sync is enabled and operating normally.
    parent_item->AddChild(ActionAppMenuManager::CreateIndirectActionItem(
        kActionShowSyncSettings, ActionAppMenuManager::DisplayType::kRow,
        ui::kColorMenuBackground,
        l10n_util::GetStringUTF16(IDS_PROFILE_ROW_SYNC_IS_ON),
        ui::ImageModel::FromVectorIcon(
            features::IsRoundedIconsEnabled()
                ? vector_icons::kSyncIcon
                : vector_icons::kSyncChromeRefreshOldIcon,
            ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize)));
  } else {
    // User is signed out or not syncing.
    if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
      if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        parent_item->AddChild(ActionAppMenuManager::CreateIndirectActionItem(
            kActionShowSignin, ActionAppMenuManager::DisplayType::kRow,
            ui::kColorMenuBackground,
            l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON),
            ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled() ? kAccountCircleFilledIcon
                                                  : kAccountCircleOldIcon,
                ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize)));
        signin_metrics::LogSignInOffered(
            signin_metrics::AccessPoint::kMenu,
            signin_ui_util::GetSingleAccountForPromos(
                identity_manager,
                AccountPreviewDataServiceFactory::GetForProfile(profile))
                    .IsEmpty()
                ? signin_metrics::PromoAction::
                      PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                : signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);
      }
    } else {
      parent_item->AddChild(ActionAppMenuManager::CreateIndirectActionItem(
          kActionTurnOnSync, ActionAppMenuManager::DisplayType::kRow,
          ui::kColorMenuBackground,
          l10n_util::GetStringUTF16(IDS_PROFILE_ROW_TURN_ON_SYNC),
          ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kSyncDisabledIcon
                  : vector_icons::kSyncOffChromeRefreshOldIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize)));
    }
  }
  return true;
#else
  return false;
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

// Populates "Other Chrome profiles" section.
void ProfileDynamicMenu::BuildOtherProfilesSection(
    actions::BaseAction* parent_item,
    Profile* profile,
    const ui::ColorProvider* color_provider) {
  const bool has_profile_manager =
      g_browser_process && g_browser_process->profile_manager();
  if (!profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
      !profile->IsEnterpriseIsolatedModeProfile()) {
    if (has_profile_manager) {
      parent_item->AddChild(ActionAppMenuManager::CreateDividerActionItem());
      parent_item->AddChild(ActionAppMenuManager::CreateHeaderActionItem(
          l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE)));

      const int avatar_icon_size =
          GetLayoutConstant(LayoutConstant::kAppMenuProfileRowAvatarIconSize);
      profiles::PlaceholderAvatarIconParams icon_params =
          GetPlaceholderAvatarIconParamsVisibleAgainstColor(
              color_provider
                  ? color_provider->GetColor(ui::kColorMenuBackground)
                  : SK_ColorWHITE);
      auto profile_entries =
          GetAllOtherProfileEntriesForProfileSubMenu(profile);
      const bool needs_separator = !profile_entries.empty();

      // Populate other existing Chrome profiles with their circular avatar and
      // callback.
      for (ProfileAttributesEntry* profile_entry : profile_entries) {
        std::u16string display_name = GetProfileMenuDisplayName(profile_entry);
        std::u16string truncated_name =
            ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
                display_name,
                GetLayoutConstant(
                    LayoutConstant::kAppMenuMaximumCharacterLength),
                gfx::CHARACTER_BREAK));

        // Circular profile avatar icon.
        ui::ImageModel avatar_model =
            ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
                profile_entry->GetAvatarIcon(
                    avatar_icon_size, /*use_high_res_file=*/true, icon_params),
                avatar_icon_size, avatar_icon_size, profiles::SHAPE_CIRCLE));

        // Optional AI gradient ring around avatar if eligible.
        bool has_ai_ring = false;
        if (base::FeatureList::IsEnabled(
                switches::kEnableAiSubscriptionAvatarRing) &&
            profile_entry->GetAiSubscriptionTier() > 0 && color_provider) {
          has_ai_ring = true;
          avatar_model =
              ui::ImageModel::FromImageSkia(AddLinearGradientRingToAvatar(
                  avatar_model, *color_provider, avatar_icon_size));
        }

        auto builder = actions::ActionItem::Builder();
        builder.SetText(truncated_name)
            .SetImage(avatar_model)
            .SetInvokeActionCallback(base::BindRepeating(
                [](base::FilePath profile_path, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  // Switching profiles opens or activates a browser window for
                  // that profile.
                  profiles::SwitchToProfile(profile_path,
                                            /*always_create=*/false);
                },
                profile_entry->GetPath()))
            .SetProperty(ActionAppMenuManager::kContainerColorKey,
                         ui::kColorMenuBackground);

        auto action_item = std::move(builder).Build();
        if (has_ai_ring && !display_name.empty()) {
          action_item->SetAccessibleName(l10n_util::GetStringFUTF16(
              IDS_PROFILE_AVATAR_NAME_WITH_AI_MEMBERSHIP, display_name));
        }
        parent_item->AddChild(std::move(action_item));
      }

      if (needs_separator) {
        parent_item->AddChild(ActionAppMenuManager::CreateDividerActionItem());
      }
    }
  }
}

// Retrieves the ColorProvider from the active window's widget.
const ui::ColorProvider* ProfileDynamicMenu::GetColorProvider() const {
  CHECK(browser_window_interface_);
  ui::BaseWindow* window = browser_window_interface_->GetWindow();
  CHECK(window);
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow());
  CHECK(widget);
  return widget->GetColorProvider();
}
