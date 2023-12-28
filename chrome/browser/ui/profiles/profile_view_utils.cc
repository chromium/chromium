// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_view_utils.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/window_open_disposition.h"
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
  return GetAvatarSyncErrorType(profile) == AvatarSyncErrorType::kSyncPaused;
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
  int browser_count = chrome::GetBrowserCount(profile);
  if (!profile->IsOffTheRecord() && profile->HasPrimaryOTRProfile()) {
    browser_count += chrome::GetBrowserCount(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
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
      profile_name, GetLayoutConstant(APP_MENU_MAXIMUM_CHARACTER_LENGTH),
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
