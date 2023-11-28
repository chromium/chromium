// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_live_test.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/supervised_user/family_member.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace supervised_user {
namespace {

// List of accounts specified in
// chrome/browser/internal/resources/signin/test_accounts.json
static constexpr std::string_view kHeadOfHouseholdAccountId{
    "FAMILY_HEAD_OF_HOUSEHOLD"};
static constexpr std::string_view kChildAccountId{"FAMILY_CHILD_1"};

Profile& CreateNewProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  return profiles::testing::CreateProfileSync(profile_manager, profile_path);
}

}  // namespace

FamilyLiveTest::FamilyLiveTest() = default;
FamilyLiveTest::FamilyLiveTest(
    const std::vector<std::string>& extra_enabled_hosts)
    : extra_enabled_hosts_(extra_enabled_hosts) {}
FamilyLiveTest::~FamilyLiveTest() = default;

/* static */ void FamilyLiveTest::TurnOnSyncFor(FamilyMember& member) {
  member.TurnOnSync();
  member.browser()->tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  member.browser()->tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void FamilyLiveTest::SetUp() {
  signin::test::LiveTest::SetUp();
  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

void FamilyLiveTest::SetUpOnMainThread() {
  signin::test::LiveTest::SetUpOnMainThread();

  child_ = MakeSignedInBrowser(kChildAccountId);
  head_of_household_ = MakeSignedInBrowser(kHeadOfHouseholdAccountId);
}

void FamilyLiveTest::SetUpInProcessBrowserTestFixture() {
  signin::test::LiveTest::SetUpInProcessBrowserTestFixture();

  for (const auto& host : extra_enabled_hosts_) {
    host_resolver()->AllowDirectLookup(host);
  }
}

signin::test::TestAccount FamilyLiveTest::GetTestAccount(
    std::string_view account_name) const {
  signin::test::TestAccount account;
  CHECK(GetTestAccountsUtil()->GetAccount(std::string(account_name), account));
  return account;
}

std::unique_ptr<FamilyMember> FamilyLiveTest::MakeSignedInBrowser(
    std::string_view account_name) {
  // Managed externally to the test fixture.
  Profile& profile = CreateNewProfile();
  Browser* browser = CreateBrowser(&profile);
  CHECK(browser) << "Expected to create a browser.";

  FamilyMember::NewTabCallback new_tab_callback = base::BindLambdaForTesting(
      [this, browser](int index, const GURL& url,
                      ui::PageTransition transition) -> bool {
        return this->AddTabAtIndexToBrowser(browser, index, url, transition);
      });

  return std::make_unique<FamilyMember>(GetTestAccount(account_name), *browser,
                                        new_tab_callback);
}

GURL FamilyLiveTest::GetRoutedUrl(std::string_view url_spec) const {
  GURL url(url_spec);

  for (std::string_view enabled_host : extra_enabled_hosts_) {
    if (url.host() == enabled_host) {
      return url;
    }
  }
  NOTREACHED_NORETURN()
      << "Supplied url_spec is not routed in this test fixture.";
}

}  // namespace supervised_user
