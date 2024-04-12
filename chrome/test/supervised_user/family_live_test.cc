// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_live_test.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/supervised_user/family_member.h"
#include "chrome/test/supervised_user/test_state_seeded_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace supervised_user {
namespace {

// List of accounts specified in
// chrome/browser/internal/resources/signin/test_accounts.json.
static constexpr std::string_view kHeadOfHouseholdAccountIdSuffix{
    "HEAD_OF_HOUSEHOLD"};
static constexpr std::string_view kChildAccountIdSuffix{"CHILD_1"};

Profile& CreateNewProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  return profiles::testing::CreateProfileSync(profile_manager, profile_path);
}

std::string GetFamilyMemberIdentifier(FamilyIdentifier family_identifier,
                                      std::string_view member_identifier) {
  return family_identifier.value() + "_" + std::string(member_identifier);
}

}  // namespace

FamilyLiveTest::FamilyLiveTest(FamilyIdentifier family_identifier)
    : family_identifier_(family_identifier) {}
FamilyLiveTest::FamilyLiveTest(
    FamilyIdentifier famiy_identifier,
    const std::vector<std::string>& extra_enabled_hosts)
    : family_identifier_(famiy_identifier),
      extra_enabled_hosts_(extra_enabled_hosts) {}
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

  child_ = MakeSignedInBrowser(
      GetFamilyMemberIdentifier(family_identifier_, kChildAccountIdSuffix));
  head_of_household_ = MakeSignedInBrowser(GetFamilyMemberIdentifier(
      family_identifier_, kHeadOfHouseholdAccountIdSuffix));
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

bool FamilyLiveTest::AccountExists(std::string_view account_name) const {
  signin::test::TestAccount account;
  return GetTestAccountsUtil()->GetAccount(std::string(account_name), account);
}

std::unique_ptr<FamilyMember> FamilyLiveTest::MakeSignedInBrowser(
    std::string_view account_name) {
  if (!AccountExists(std::string(account_name))) {
    return nullptr;
  }

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

InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyIdentifier family_identifier)
    : InteractiveBrowserTestT<FamilyLiveTest>(family_identifier) {}
InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyIdentifier family_identifier,
    const std::vector<std::string>& extra_enabled_hosts)
    : InteractiveBrowserTestT<FamilyLiveTest>(family_identifier,
                                              extra_enabled_hosts) {}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveFamilyLiveTest::DefineChromeTestState(
    ui::test::StateIdentifier<ChromeTestStateObserver> id,
    const std::vector<GURL>& allowed_urls,
    const std::vector<GURL>& blocked_urls) {
  return Steps(
      Log("DefineChromeTestState: start."),
      ObserveState(id, std::make_unique<DefineChromeTestStateObserver>(
                           child(), allowed_urls, blocked_urls)),
      If(base::BindOnce(&UrlFiltersAreConfigured, std::ref(child()),
                        allowed_urls, blocked_urls),
         /* then= */ Log("DefineChromeTestState: not needed."),
         /* else= */
         Steps(
             // This delay reduces tests flakiness on picking up changes
             // resulting from the RPC call.
             Do([]() { Delay(base::Seconds(2)); }),
             Log("DefineChromeTestState: performing rpc..."), Do([&]() {
               IssueDefineTestStateOrDie(head_of_household(), child(),
                                         allowed_urls, blocked_urls);
             }),
             Log("DefineChromeTestState: waiting for state change..."),
             WaitForState(id, ChromeTestStateSeedingResult::kIntendedState),
             Log("DefineChromeTestState: done."))),
      StopObservingState(id));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveFamilyLiveTest::ResetChromeTestState(
    ui::test::StateIdentifier<ChromeTestStateObserver> id) {
  return Steps(
      Log("ResetChromeTestState: start."),
      ObserveState(id, std::make_unique<ResetChromeTestStateObserver>(child())),
      If(base::BindOnce(&UrlFiltersAreEmpty, std::ref(child())),
         /* then= */ Log("ResetChromeTestState: not needed."),
         /* else= */
         Steps(
             // This delay reduces tests flakiness on picking up changes
             // resulting from the RPC call.
             Do([]() { Delay(base::Seconds(2)); }),
             Log("ResetChromeTestState: performing rpc..."),
             Do([&]() { IssueResetOrDie(head_of_household(), child()); }),
             Log("ResetChromeTestState: waiting for state change..."),
             WaitForState(id, ChromeTestStateSeedingResult::kIntendedState),
             Log("ResetChromeTestState: done."))),
      StopObservingState(id));
}

}  // namespace supervised_user
