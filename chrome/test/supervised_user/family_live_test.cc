// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_live_test.h"

#include <ostream>
#include <string>
#include <string_view>
#include <utility>
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
#include "chrome/test/supervised_user/browser_user.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/test_support/account_repository.h"
#include "components/supervised_user/test_support/family_link_settings_state_management.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

Profile& CreateNewProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  return profiles::testing::CreateProfileSync(profile_manager, profile_path);
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

test_accounts::FamilyMember CreateTestAccountFromCredentialsSwitch(
    std::string_view credentials_switch) {
  std::string credentials =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          credentials_switch);
  test_accounts::FamilyMember member;

  if (credentials_switch == kHeadOfHouseholdCredentialsSwitch) {
    member.role = kidsmanagement::FamilyRole::HEAD_OF_HOUSEHOLD;
  } else if (credentials_switch == kChildCredentialsSwitch) {
    member.role = kidsmanagement::FamilyRole::CHILD;
  } else {
    NOTREACHED() << "Unknown credentials switch: " << credentials_switch;
  }

  std::string username, password;
  if (RE2::FullMatch(credentials, "(.*):(.*)", &member.username,
                     &member.password)) {
    return member;
  }

  NOTREACHED() << "Expected username:password format, but got: " << credentials;
}
}  // namespace

FamilyLiveTest::FamilyLiveTest(RpcMode rpc_mode) : rpc_mode_(rpc_mode) {}
FamilyLiveTest::FamilyLiveTest(
    RpcMode rpc_mode,
    const std::vector<std::string_view>& extra_enabled_hosts)
    : extra_enabled_hosts_(extra_enabled_hosts.begin(),
                           extra_enabled_hosts.end()),
      rpc_mode_(rpc_mode) {}

FamilyLiveTest::~FamilyLiveTest() = default;

BrowserUser& FamilyLiveTest::head_of_household() const {
  CHECK(head_of_household_)
      << "No head of household found for given family or credentials";
  return *head_of_household_;
}

BrowserUser& FamilyLiveTest::child() const {
  CHECK(child_) << "No child found for given family or credentials";
  return *child_;
}

BrowserUser& FamilyLiveTest::rpc_issuer() const {
  switch (rpc_mode_) {
    case RpcMode::kProd:
      return head_of_household();
    case RpcMode::kTestImpersonation:
      return child();
  }
  NOTREACHED();
}

void FamilyLiveTest::TurnOnSync() {
  TurnOnSyncFor(*head_of_household_);
  TurnOnSyncFor(*child_);
}

void FamilyLiveTest::TurnOnSyncFor(BrowserUser& browser_user) {
  browser_user.TurnOnSync();
  browser_user.browser().tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  browser_user.browser().tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);

  if (IsSwitchEnabled(kDebugSwitch)) {
    CHECK(AddTabAtIndexToBrowser(&browser_user.browser(), 1,
                                 GURL("chrome://sync-internals"),
                                 ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
  }

  if (IsSwitchEnabled(kWaitForSyncInvalidationReadySwitch)) {
    // After turning the sync on, wait until this is fully initialized.
    LOG(INFO) << "Waiting for sync service to set up invalidations.";
    syncer::SyncServiceImpl* service =
        SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
            &browser_user.profile());
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
  gfx::ScopedAnimationDurationScaleMode disable_animation(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

void FamilyLiveTest::SetUpOnMainThread() {
  signin::test::LiveTest::SetUpOnMainThread();

  if (IsSwitchEnabled(kFamilyFeatureIdentifierSwitch) &&
      IsSwitchEnabled(kAccountRepositoryPath)) {
    // Family from static test_accounts file mode
    CHECK(!IsSwitchEnabled(kHeadOfHouseholdCredentialsSwitch))
        << "Head of household credentials are ignored if "
        << kFamilyFeatureIdentifierSwitch << " and " << kAccountRepositoryPath
        << " are set";
    CHECK(!IsSwitchEnabled(kChildCredentialsSwitch))
        << "Child credentials are ignored if " << kFamilyFeatureIdentifierSwitch
        << " and " << kAccountRepositoryPath << " are set";

    std::optional<test_accounts::Feature> family_feature =
        test_accounts::ParseFeature(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                kFamilyFeatureIdentifierSwitch));
    CHECK(family_feature.has_value()) << "Unrecognized family feature";

    TestAccountRepository repository(
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kAccountRepositoryPath));

    std::optional<test_accounts::Family> family =
        repository.GetRandomFamilyByFeature(*family_feature);
    CHECK(family_feature.has_value())
        << "Family with requested feature not available";

    std::optional<test_accounts::FamilyMember> head_of_household =
        GetFirstFamilyMemberByRole(
            *family, kidsmanagement::FamilyRole::HEAD_OF_HOUSEHOLD);
    CHECK(head_of_household.has_value()) << "Head of household not available";
    SetHeadOfHousehold(*head_of_household);

    std::optional<test_accounts::FamilyMember> child =
        GetFirstFamilyMemberByRole(*family, kidsmanagement::FamilyRole::CHILD);
    CHECK(child.has_value()) << "Child not available";
    SetChild(*child);
    return;
  }

  if (IsSwitchEnabled(kHeadOfHouseholdCredentialsSwitch) &&
      IsSwitchEnabled(kChildCredentialsSwitch)) {
    SetHeadOfHousehold(CreateTestAccountFromCredentialsSwitch(
        kHeadOfHouseholdCredentialsSwitch));
    SetChild(CreateTestAccountFromCredentialsSwitch(kChildCredentialsSwitch));
    return;
  }

  NOTREACHED() << "Either specify " << kFamilyFeatureIdentifierSwitch << " and "
               << kAccountRepositoryPath << " or configure credentials using "
               << kHeadOfHouseholdCredentialsSwitch << " and "
               << kChildCredentialsSwitch << ".";
}
void FamilyLiveTest::TearDownOnMainThread() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  for (const BrowserUser* user : {child_.get(), head_of_household_.get()}) {
    if (!user) {
      continue;
    }
    // Signs out the account, so the server is notified to no longer attempt to
    // notify this client. Explicit sign-out is critical, otherwise server-side
    // data structures can still think that the current client should receive
    // sync updates.
    user->browser().GetFeatures().signin_view_controller()->ShowGaiaLogoutTab(
        signin_metrics::SourceForRefreshTokenOperation::
            kUserMenu_SignOutAllAccounts);
  }
#endif

  head_of_household_.reset();
  child_.reset();
  signin::test::LiveTest::TearDownOnMainThread();
}

void FamilyLiveTest::SetHeadOfHousehold(
    const test_accounts::FamilyMember& credentials) {
  head_of_household_ = MakeSignedInBrowser(credentials);
}

void FamilyLiveTest::SetChild(const test_accounts::FamilyMember& credentials) {
  child_ = MakeSignedInBrowser(credentials);
}

void FamilyLiveTest::SetUpInProcessBrowserTestFixture() {
  signin::test::LiveTest::SetUpInProcessBrowserTestFixture();

  for (const auto& host : extra_enabled_hosts_) {
    host_resolver()->AllowDirectLookup(host);
  }
}

std::unique_ptr<BrowserUser> FamilyLiveTest::MakeSignedInBrowser(
    const test_accounts::FamilyMember& credentials) {
  // Managed externally to the test fixture.
  Profile& profile = CreateNewProfile();
  Browser* browser = CreateBrowser(&profile);
  CHECK(browser) << "Expected to create a browser.";

  BrowserUser::NewTabCallback new_tab_callback = base::BindLambdaForTesting(
      [this, browser](int index, const GURL& url,
                      ui::PageTransition transition) -> bool {
        return this->AddTabAtIndexToBrowser(browser, index, url, transition);
      });

  return std::make_unique<BrowserUser>(
      credentials,
      profile.GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *IdentityManagerFactory::GetForProfile(&profile), *browser, profile,
      new_tab_callback);
}

GURL FamilyLiveTest::GetRoutedUrl(std::string_view url_spec) const {
  GURL url(url_spec);

  for (std::string_view enabled_host : extra_enabled_hosts_) {
    if (url.GetHost() == enabled_host) {
      return url;
    }
  }
  NOTREACHED() << "Supplied url_spec is not routed in this test fixture.";
}

InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyLiveTest::RpcMode rpc_mode)
    : InteractiveBrowserTestMixin<FamilyLiveTest>(rpc_mode) {}
InteractiveFamilyLiveTest::InteractiveFamilyLiveTest(
    FamilyLiveTest::RpcMode rpc_mode,
    const std::vector<std::string_view>& extra_enabled_hosts)
    : InteractiveBrowserTestMixin<FamilyLiveTest>(rpc_mode,
                                                  extra_enabled_hosts) {}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveFamilyLiveTest::WaitForStateSeeding(
    ui::test::StateIdentifier<InIntendedStateObserver> id,
    const BrowserUser& browser_user,
    const FamilyLinkSettingsState& state) {
  return Steps(
      Log(base::StrCat({"WaitForState[", state.ToString(), "]: start"})),
      If([&]() { return !state.Check(browser_user.GetServices()); },
         Then(
             Do([&]() {
               state.Seed(rpc_issuer().identity_manager(),
                          rpc_issuer().url_loader_factory(),
                          browser_user.GetAccountId().ToString());
             }),
             PollState(
                 id,
                 [&]() {
                   SyncServiceFactory::GetForProfile(&browser_user.profile())
                       ->TriggerRefresh(
                           syncer::SyncService::TriggerRefreshSource::kUnknown,
                           syncer::DataTypeSet::All());
                   return state.Check(browser_user.GetServices());
                 },
                 /*polling_interval=*/base::Seconds(2)),
             WaitForState(id, true), StopObservingState(id)),
         Else(Log(base::StrCat(
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
  NOTREACHED();
}

}  // namespace supervised_user
