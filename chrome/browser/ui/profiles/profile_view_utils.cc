// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_view_utils.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/incognito_allowed_url.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/sync/service/sync_service.h"
#include "components/user_prefs/user_prefs.h"
#include "net/base/url_util.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

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

bool IsSyncPaused(Profile* profile) {
  const syncer::SyncService* service =
      SyncServiceFactory::GetForProfile(profile);
  // Avoid returning true in case of no sync consent, as kSignInPending should
  // be handled differently.
  return service &&
         service->GetUserActionableError() ==
             syncer::SyncService::UserActionableError::kSignInNeedsUpdate &&
         IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
             signin::ConsentLevel::kSync);
}

bool HasUnconstentedProfile(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return identity_manager ? !profile->IsGuestSession() &&
                                identity_manager->HasPrimaryAccount(
                                    signin::ConsentLevel::kSignin)
                          : false;
}

int CountBrowsersFor(Profile* profile) {
  if (!profile || !ProfileBrowserCollection::GetForProfile(profile)) {
    return 0;
  }
  int browser_count =
      ProfileBrowserCollection::GetForProfile(profile)->GetSize();
  if (!profile->IsOffTheRecord() && profile->HasPrimaryOTRProfile()) {
    browser_count +=
        ProfileBrowserCollection::GetForProfile(
            profile->GetPrimaryOTRProfile(/*create_if_needed=*/true))
            ->GetSize();
  }
  return browser_count;
}

AccountInfo GetAccountInfoFromProfile(const Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  // IdentityManager may be null if one is not mapped to the profile through the
  // KeyedServiceFactory. We do not create one if it doesn't already exist and
  // simply return an empty AccountInfo object.
  if (!identity_manager) {
    return AccountInfo();
  }
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return identity_manager->FindExtendedAccountInfo(account);
}

ProfileAttributesEntry* GetProfileAttributesFromProfile(
    const Profile* profile) {
  return g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile->GetPath());
}

std::u16string GetProfileMenuDisplayName(
    ProfileAttributesEntry* profile_attributes) {
  std::u16string profile_name = profile_attributes->GetName();
  if (profile_name.empty()) {
    profile_name = profile_attributes->GetLocalProfileName();
  }
  profile_name = ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
      profile_name,
      GetLayoutConstant(LayoutConstant::kAppMenuMaximumCharacterLength),
      gfx::CHARACTER_BREAK));

  return profile_name;
}

std::vector<ProfileAttributesEntry*> GetAllOtherProfileEntriesForProfileSubMenu(
    const Profile* current_profile) {
  auto profile_entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByLocalProfileNameWithCheck();

  std::vector<ProfileAttributesEntry*> result;

  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile and omitted profiles are excluded.
    if (profile_entry->GetPath() == current_profile->GetPath() ||
        profile_entry->IsOmitted()) {
      continue;
    }

    result.push_back(profile_entry);
  }

  return result;
}

bool IsOtherProfileCommand(int command_id) {
  return command_id >= AppMenuModel::kMinOtherProfileCommandId &&
         ((command_id - IDC_FIRST_UNBOUNDED_MENU) %
              AppMenuModel::kNumUnboundedMenuTypes ==
          (AppMenuModel::kMinOtherProfileCommandId - IDC_FIRST_UNBOUNDED_MENU));
}

bool IsOpenLinkOTREnabled(Profile* source_profie, const GURL& url) {
  if (source_profie->IsOffTheRecord() || !url.is_valid()) {
    return false;
  }

  if (!IsURLAllowedInIncognito(url)) {
    return false;
  }

  policy::IncognitoModeAvailability incognito_avail =
      IncognitoModePrefs::GetAvailability(
          user_prefs::UserPrefs::Get(source_profie));
  return incognito_avail != policy::IncognitoModeAvailability::kDisabled;
}

bool IsAiSubscriptionRingEnabled(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          features::kEnableAiSubscriptionAvatarRing)) {
    return false;
  }
  if (!profile) {
    return false;
  }
  // TODO(crbug.com/522296672): Specify the right way to obtain this information
  // as `GetAiSubscriptionTier` only works for certain groups of users.
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(profile);
  return subscription_service &&
         subscription_service->GetAiSubscriptionTier() > 0;
}

gfx::ImageSkia AddAiRingToAvatar(const ui::ImageModel& avatar_image,
                                 const ui::ColorProvider& color_provider,
                                 int avatar_size,
                                 int gap_width,
                                 int ring_thickness) {
  // Gradient stops corresponding to SVG:
  // 1) 0 to 85%: Solid start_color
  // 2) 85% to 99.6%: Linear transition between start and end color.
  // 3) 99.6% to 100%: Solid end_color.
  constexpr float kPositions[] = {0.0f, 0.85f, 0.995943f, 1.0f};

  return profiles::AddAiRingToAvatar(
      avatar_image, color_provider,
      color_provider.GetColor(kColorAiSubscriptionRingGradientStart),
      color_provider.GetColor(kColorAiSubscriptionRingGradientEnd), kPositions,
      avatar_size, gap_width, ring_thickness);
}
