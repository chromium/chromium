// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_member.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/test_support/browser_state_management.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

FamilyMember::FamilyMember(
    signin::TestAccountSigninCredentials credentials,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager,
    Browser& browser,
    Profile& profile,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : account_(credentials),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      browser_(browser),
      profile_(profile),
      sign_in_functions_(base::BindLambdaForTesting(
                             [&browser]() -> Browser* { return &browser; }),
                         add_tab_function) {}
FamilyMember::~FamilyMember() = default;

void FamilyMember::TurnOnSync() {
  sign_in_functions_.TurnOnSync(account_, 0);
}

void FamilyMember::SignOutFromWeb() {
  sign_in_functions_.SignOutFromWeb();
}

BrowserState::Services FamilyMember::GetServices() const {
  return {
      *SupervisedUserServiceFactory::GetForProfile(&profile_.get()),
      *profile_->GetPrefs(),
      *HostContentSettingsMapFactory::GetForProfile(&profile_.get()),
  };
}

CoreAccountId FamilyMember::GetAccountId() const {
  CHECK(supervised_user::IsPrimaryAccountSubjectToParentalControls(
            &identity_manager_.get()) == signin::Tribool::kTrue)
      << "Blocklist control page is only available to user who have that "
         "feature enabled. Check if member is a subject to parental controls. "
         "Account: "
      << account_.user;

  return identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

std::string_view FamilyMember::GetAccountPassword() const {
  return account_.password;
}

}  // namespace supervised_user
