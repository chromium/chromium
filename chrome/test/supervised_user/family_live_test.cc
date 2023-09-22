// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_live_test.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/supervised_user/family_member_browser.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace supervised_user {
namespace {

// List of accounts specified in
// chrome/browser/internal/resources/signin/test_accounts.json
static constexpr std::string_view kHeadOfHouseholdAccountId{"TEST_ACCOUNT_1"};
static constexpr std::string_view kChildAccountId{"TEST_ACCOUNT_2"};

Profile& CreateNewProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  return profiles::testing::CreateProfileSync(profile_manager, profile_path);
}

}  // namespace

FamilyLiveTest::FamilyLiveTest() = default;
FamilyLiveTest::~FamilyLiveTest() = default;

void FamilyLiveTest::SetUp() {
  signin::test::LiveTest::SetUp();
  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

void FamilyLiveTest::SetUpOnMainThread() {
  signin::test::LiveTest::SetUpOnMainThread();

  head_of_household_ = MakeSignedInBrowser(kHeadOfHouseholdAccountId);
  child_ = MakeSignedInBrowser(kChildAccountId);
}

signin::test::TestAccount FamilyLiveTest::GetTestAccount(
    std::string_view account_name) const {
  signin::test::TestAccount account;
  CHECK(GetTestAccountsUtil()->GetAccount(std::string(account_name), account));
  return account;
}

std::unique_ptr<FamilyMemberBrowser> FamilyLiveTest::MakeSignedInBrowser(
    std::string_view account_name) {
  // Managed externally to the test fixture.
  Profile& profile = CreateNewProfile();
  Browser* browser = CreateBrowser(&profile);
  CHECK(browser) << "Expected to create a browser.";

  FamilyMemberBrowser::NewTabCallback new_tab_callback =
      base::BindLambdaForTesting([this, browser](
                                     int index, const GURL& url,
                                     ui::PageTransition transition) -> bool {
        return this->AddTabAtIndexToBrowser(browser, index, url, transition);
      });

  std::unique_ptr<FamilyMemberBrowser> family_member_browser =
      std::make_unique<FamilyMemberBrowser>(GetTestAccount(account_name),
                                            *browser, new_tab_callback);
  family_member_browser->SignIn();
  return family_member_browser;
}

}  // namespace supervised_user
