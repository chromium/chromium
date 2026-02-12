// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

namespace {

constexpr base::TimeDelta kForcedSigninToastDelay = base::Seconds(5);

constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";
constexpr char kForcedMigrationAccountManagedHistogram[] =
    "Signin.ForcedDiceMigration.HasAcceptedAccountManagement";
constexpr char kForcedSigninBrowserInstanceAvailableAfterTimerHistogram[] =
    "Signin.ForcedDiceMigration.BrowserInstanceAvailableAfterTimer";

bool IsUserEligibleForDiceMigration(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // The user is not signed in or has sync enabled.
    return false;
  }

  // The user is implicitly signed in.
  return !profile->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin);
}

bool MaybeMigrateUser(Profile* profile) {
  if (!IsUserEligibleForDiceMigration(profile)) {
    return false;
  }
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);

  // TODO(crbug.com/399838468): Consider calling
  // `PrimaryAccountManager::SetExplicitBrowserSigninPrefs` upon explicit signin
  // pref change.
  prefs->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                    true);

  prefs->SetBoolean(prefs::kExplicitBrowserSignin, true);

  // Mark the migration pref as successful.
  prefs->SetBoolean(kDiceMigrationMigrated, true);
  return true;
}

bool MaybeShowToast(Browser* browser) {
  ToastController* const toast_controller =
      browser->browser_window_features()->toast_controller();
  if (!toast_controller) {
    return false;
  }
  toast_controller->MaybeShowToast(ToastParams(ToastId::kDiceUserMigrated));
  return true;
}

}  // namespace

const char kDiceMigrationMigrated[] = "signin.dice_migration.migrated";

DiceMigrationService::DiceMigrationService(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
  // Force migration all implicitly signed-in users.
  const bool migrated = ForceMigrateUserIfEligible();
  base::UmaHistogramBoolean(kForceMigratedHistogram, migrated);
  // By now, the user should have been force migrated and is no longer in the
  // DICe state.
  CHECK(!IsUserEligibleForDiceMigration(profile_));
}

DiceMigrationService::~DiceMigrationService() = default;

// static
void DiceMigrationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kDiceMigrationMigrated, false);
}

base::OneShotTimer& DiceMigrationService::GetToastTriggerTimerForTesting() {
  return toast_trigger_timer_;
}

bool DiceMigrationService::ForceMigrateUserIfEligible() {
  if (!IsUserEligibleForDiceMigration(profile_)) {
    return false;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  const bool has_accepted_account_management =
      enterprise_util::UserAcceptedAccountManagement(profile_);
  base::UmaHistogramBoolean(kForcedMigrationAccountManagedHistogram,
                            has_accepted_account_management);
  if (!has_accepted_account_management) {
    // This is either a consumer account or an enterprise account that has not
    // accepted the account management.
    // Remove the primary account, but keep the tokens. This will make the user
    // signed in only to the web.
    CHECK(identity_manager->GetPrimaryAccountMutator()
              ->RemovePrimaryAccountButKeepTokens(
                  signin_metrics::ProfileSignout::kForcedDiceMigration));
    return true;
  }
  // The user is an enterprise account that has accepted the account management.
  // Such users cannot be signed out. Migrate these users to explicitly
  // signed-in state.
  CHECK(MaybeMigrateUser(profile_));

  // Trigger the timer to show the toast.
  auto show_toast = [](Profile* profile) {
    Browser* browser = chrome::FindBrowserWithProfile(profile);
    base::UmaHistogramBoolean(
        kForcedSigninBrowserInstanceAvailableAfterTimerHistogram, browser);
    if (browser) {
      CHECK(MaybeShowToast(browser));
    } else {
      // The profile is under creation and hence no browser instance is tied to
      // it yet. Wait for the browser to be created before trying to show the
      // toast.
      // This object deletes itself when done.
      new profiles::BrowserAddedForProfileObserver(
          profile, base::BindOnce(base::IgnoreResult(&MaybeShowToast)));
    }
  };
  CHECK(!toast_trigger_timer_.IsRunning());
  toast_trigger_timer_.Start(FROM_HERE, kForcedSigninToastDelay,
                             base::BindOnce(std::move(show_toast), profile_));
  return true;
}
