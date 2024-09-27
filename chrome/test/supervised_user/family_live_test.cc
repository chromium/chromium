// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_live_test.h"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/invalidations/invalidations_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/supervised_user/test_support/browser_state_management.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

// When enabled the tests explicitly wait for sync invalidation to be ready.
const char* kWaitForSyncInvalidationReadySwitch =
    "supervised-tests-wait-for-sync-invalidation-ready";
// When enabled, the browser opens extra debugging tabs & the logging is more
// detailed.
const char* kDebugSwitch = "supervised-tests-debug-features";

bool IsSwitchEnabled(const char* flag) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(flag);
}

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

std::string GetFamilyIdentifier() {
  const base::CommandLine* const cmd = base::CommandLine::ForCurrentProcess();
  CHECK(cmd->HasSwitch(kFamilyIdentifierSwitch))
      << "Please specify " << kFamilyIdentifierSwitch << " switch";
  return cmd->GetSwitchValueASCII(kFamilyIdentifierSwitch);
}

std::string GetFamilyMemberIdentifier(std::string_view member_identifier) {
  return GetFamilyIdentifier() + "_" + std::string(member_identifier);
}

bool HasAuthError(syncer::SyncServiceImpl* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncSetupChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync setup to complete";
    if (service()->GetTransportState() ==
            syncer::SyncService::TransportState::ACTIVE &&
        service()->IsSyncFeatureActive()) {
      return true;
    }
    // Sync is blocked by an auth error.
    if (HasAuthError(service())) {
      return true;
    }

    // Still waiting on sync setup.
    return false;
  }
};

signin::TestAccountSigninCredentials CreateTestAccountFromCredentialsSwitch(
    std::string_view credentials_switch) {
  std::string credentials =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          credentials_switch);
  std::string username, password;
  if (RE2::FullMatch(credentials, "(.*):(.*)", &username, &password)) {
    return {username, password};
  }

  NOTREACHED_NORETURN() << "Expected username:password format, but got: "
                        << credentials;
}
}  // namespace

FamilyLiveTest::FamilyLiveTest(RpcMode rpc_mode) : rpc_mode_(rpc_mode) {}
FamilyLiveTest::FamilyLiveTest(
    RpcMode rpc_mode,
    const std::vector<std::string>& extra_enabled_hosts)
    : extra_enabled_hosts_(extra_enabled_hosts), rpc_mode_(rpc_mode) {}
FamilyLiveTest::~FamilyLiveTest() = default;

FamilyMember& FamilyLiveTest::head_of_household() const {
  CHECK(head_of_household_)
      << "No head of household found for given family or credentials";
  return *head_of_household_;
}

FamilyMember& FamilyLiveTest::child() const {
  CHECK(child_) << "No child found for given family or credentials";
  return *child_;
}

FamilyMember& FamilyLiveTest::rpc_issuer() const {
  switch (rpc_mode_) {
    case RpcMode::kProd:
      return head_of_household();
    case RpcMode::kTestImpersonation:
      return child();
  }
  NOTREACHED_NORETURN();
}

void FamilyLiveTest::TurnOnSync() {
  TurnOnSyncFor(*head_of_household_);
  TurnOnSyncFor(*child_);
}

void FamilyLiveTest::TurnOnSyncFor(FamilyMember& member) {
  member.TurnOnSync();
  member.browser().tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  member.browser().tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);

  if (IsSwitchEnabled(kDebugSwitch)) {
    CHECK(AddTabAtIndexToBrowser(&member.browser(), 1,
                                 GURL("chrome://sync-internals"),
                                 ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
  }

  if (IsSwitchEnabled(kWaitForSyncInvalidationReadySwitch)) {
    // After turning the sync on, wait until this is fully initialized.
    LOG(INFO) << "Waiting for sync service to set up invalidations.";
    syncer::SyncServiceImpl* service =
        SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
            &member.profile());
    service->SetInvalidationsForSessionsEnabled(true);
    CHECK(SyncSetupChecker(service).Wait()) << "SyncSetupChecker timed out.";
    CHECK(InvalidationsStatusChecker(service, /*expected_status=*/true).Wait())
        << "Invalidation checker timed out.";
    LOG(INFO) << "Invalidations ready.";
  }
}

void FamilyLiveTest::SetUp() {
  signin::test::LiveTest::SetUp();
  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

void FamilyLiveTest::SetUpOnMainThread() {
  signin::test::LiveTest::SetUpOnMainThread();

  if (IsSwitchEnabled(kFamilyIdentifierSwitch)) {
    // Family from static test_accounts file mode
    CHECK(!IsSwitchEnabled(kHeadOfHouseholdCredentialsSwitch))
        << "Head of household credentials are ignored if "
        << kFamilyIdentifierSwitch << " is set";
    CHECK(!IsSwitchEnabled(kChildCredentialsSwitch))
        << "Child credentials are ignored if " << kFamilyIdentifierSwitch
        << " is set";

    SetHeadOfHousehold(GetAccountFromFile(kHeadOfHouseholdAccountIdSuffix));
    SetChild(GetAccountFromFile(kChildAccountIdSuffix));
    return;
  }

  if (IsSwitchEnabled(kHeadOfHouseholdCredentialsSwitch) &&
      IsSwitchEnabled(kChildCredentialsSwitch)) {
    SetHeadOfHousehold(CreateTestAccountFromCredentialsSwitch(
        kHeadOfHouseholdCredentialsSwitch));
    SetChild(CreateTestAccountFromCredentialsSwitch(kChildCredentialsSwitch));
    return;
  }

  NOTREACHED() << "Either specify " << kFamilyIdentifierSwitch
               << " or configure credentials using "
               << kHeadOfHouseholdCredentialsSwitch << " and "
               << kChildCredentialsSwitch << ".";
}
void FamilyLiveTest::TearDownOnMainThread() {
  head_of_household_.reset();
  child_.reset();
  signin::test::LiveTest::TearDownOnMainThread();
}

void FamilyLiveTest::SetHeadOfHousehold(
    const signin::TestAccountSigninCredentials& account) {
  head_of_household_ = MakeSignedInBrowser(account);
}

void FamilyLiveTest::SetChild(
    const signin::TestAccountSigninCredentials& account) {
  child_ = MakeSignedInBrowser(account);
}

void FamilyLiveTest::SetUpInProcessBrowserTestFixture() {
  signin::test::LiveTest::SetUpInProcessBrowserTestFixture();

  for (const auto& host : extra_enabled_hosts_) {
    host_resolver()->AllowDirectLookup(host);
  }
}

signin::TestAccountSigninCredentials FamilyLiveTest::GetAccountFromFile(
    std::string_view account_name_suffix) const {
  std::optional<signin::TestAccountSigninCredentials> account =
      GetTestAccounts()->GetAccount(
          GetFamilyMemberIdentifier(account_name_suffix));
  CHECK(account.has_value());
  return *account;
}

std::unique_ptr<FamilyMember> FamilyLiveTest::MakeSignedInBrowser(
    const signin::TestAccountSigninCredentials& account) {
  // Managed externally to the test fixture.
  Profile& profile = CreateNewProfile();
  Browser* browser = CreateBrowser(&profile);
  CHECK(browser) << "Expected to create a browser.";

  FamilyMember::NewTabCallback new_tab_callback = base::BindLambdaForTesting(
      [this, browser](int index, const GURL& url,
                      ui::PageTransition transition) -> bool {
        return this->AddTabAtIndexToBrowser(browser, index, url, transition);
      });

  return std::make_unique<FamilyMember>(
      account,
      profile.GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *IdentityManagerFactory::GetForProfile(&profile), *browser, profile,
      new_tab_callback);
}

GURL FamilyLiveTest::GetRoutedUrl(std::string_view url_spec) const {
  GURL url(url_spec);

  for (std::string_view enabled_host : extra_enabled_hosts_) {
    if (url.host() == enabled_host) {
      return url;
    }
  }
  NOTREACHED() << "Supplied url_spec is not routed in this test fixture.";
}

InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyLiveTest::RpcMode rpc_mode)
    : InteractiveBrowserTestT<FamilyLiveTest>(rpc_mode) {}
InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyLiveTest::RpcMode rpc_mode,
    const std::vector<std::string>& extra_enabled_hosts)
    : InteractiveBrowserTestT<FamilyLiveTest>(rpc_mode, extra_enabled_hosts) {}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveFamilyLiveTest::WaitForStateSeeding(
    ui::test::StateIdentifier<InIntendedStateObserver> id,
    const FamilyMember& browser_user,
    const BrowserState& state) {
  return Steps(
      Log(base::StrCat({"WaitForState[", state.ToString(), "]: start"})),
      If([&]() { return !state.Check(browser_user.GetServices()); },
         /*then_steps=*/
         Steps(
             Do([&]() {
               state.Seed(rpc_issuer().identity_manager(),
                          rpc_issuer().url_loader_factory(),
                          browser_user.GetAccountId().ToString());
             }),
             PollState(
                 id, [&]() { return state.Check(browser_user.GetServices()); },
                 /*polling_interval=*/base::Seconds(2)),
             WaitForState(id, true), StopObservingState(id)),
         /*else_steps=*/
         Steps(Log(base::StrCat(
             {"WaitForState[", state.ToString(), "]: seeding skipped"})))),
      Log(base::StrCat({"WaitForState[", state.ToString(), "]: completed"})));
}

std::string ToString(FamilyLiveTest::RpcMode rpc_mode) {
  switch (rpc_mode) {
    case FamilyLiveTest::RpcMode::kProd:
      return "ProdRpcMode";
    case FamilyLiveTest::RpcMode::kTestImpersonation:
      return "TestImpersonationRpcMode";
  }
  NOTREACHED_NORETURN();
}

}  // namespace supervised_user
