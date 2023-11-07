// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/supervised_user/api_mock_setup_mixin.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"

namespace supervised_user {

namespace {
constexpr base::StringPiece kKidsManagementServiceEndpoint{
    "kidsmanagement.googleapis.com"};

// Self-consistent conditional RAII lock on list family members load.
//
// Registers to observe a preference and blocks until it is loaded for
// *supervised users* (see ~FamilyFetchedLock() and IsSupervisedProfile()).
// Effectivelly, halts the main testing thread until the first fetch of list
// family members has finished, which is typically invoked by the browser after
// startup of the SupervisedUserService.
//
// For non-supervised users, this is no-op (it just registers and unregisters a
// preference observer).
class FamilyFetchedLock {
 public:
  FamilyFetchedLock() = delete;
  explicit FamilyFetchedLock(raw_ptr<InProcessBrowserTest> test_base)
      : test_base_(test_base) {
    CHECK(test_base->browser()->profile())
        << "Must be acquitted and initialized after the profile was "
           "initialized too.";
    pref_change_registrar_.Init(GetPrefService());
    pref_change_registrar_.Add(
        std::string(prefs::kSupervisedUserCustodianName),
        base::BindRepeating(&FamilyFetchedLock::OnPreferenceRegistered,
                            base::Unretained(this)));
  }
  ~FamilyFetchedLock() {
    if (IsSupervisedProfile()) {
      base::RunLoop run_loop;
      done_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    pref_change_registrar_.RemoveAll();
  }

  FamilyFetchedLock(const FamilyFetchedLock&) = delete;
  FamilyFetchedLock& operator=(const FamilyFetchedLock&) = delete;

 private:
  void OnPreferenceRegistered() { std::move(done_).Run(); }

  // Profile::AsTestingProfile won't return TestingProfile at this stage of
  // setup, so TestingProfile::IsChild is not available yet.
  bool IsSupervisedProfile() const {
    return GetPrefService()->GetString(prefs::kSupervisedUserId) ==
           supervised_user::kChildAccountSUID;
  }

  PrefService* GetPrefService() const {
    return test_base_->browser()->profile()->GetPrefs();
  }

  raw_ptr<InProcessBrowserTest> test_base_;
  base::OnceClosure done_;
  PrefChangeRegistrar pref_change_registrar_;
};
}  // namespace

KidsManagementApiMockSetupMixin::KidsManagementApiMockSetupMixin(
    InProcessBrowserTestMixinHost& host,
    InProcessBrowserTest* test_base)
    : InProcessBrowserTestMixin(&host), test_base_(test_base) {
  SetHttpEndpointsForKidsManagementApis(feature_list_,
                                        kKidsManagementServiceEndpoint);
}
KidsManagementApiMockSetupMixin::~KidsManagementApiMockSetupMixin() = default;

void KidsManagementApiMockSetupMixin::SetUp() {
  api_mock_.InstallOn(embedded_test_server_);
  CHECK(embedded_test_server_.InitializeAndListen());
}

void KidsManagementApiMockSetupMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  CHECK(embedded_test_server_.Started());

  std::string target = embedded_test_server_.host_port_pair().ToString();

  // TODO(b/300129765): Remove manual deduplication.
  // Workaround on problem where supervision_mixin has two submixins, where both
  // of them need to alter host resolver rules. For some reason,
  // host_resolver()->AddRule() is innefective, and
  // base::CommandLine::AppendSwitch only respects the ultimate value.
  std::string previous_switch_value =
      command_line->GetSwitchValueASCII(network::switches::kHostResolverRules);
  if (!previous_switch_value.empty()) {
    base::StrAppend(&previous_switch_value, {","});
  }

  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::JoinString({previous_switch_value, "MAP",
                        kKidsManagementServiceEndpoint, target},
                       " "));

  LOG(INFO) << "Kids management api server is listening on " << target << ".";
  LOG(INFO) << "\tAll requests to [" << kKidsManagementServiceEndpoint
            << "] will be mapped to it.";
}

void KidsManagementApiMockSetupMixin::SetUpOnMainThread() {
  // If expected, halts test until initial fetch is completed.
  FamilyFetchedLock conditional_lock(test_base_);
  embedded_test_server_.StartAcceptingConnections();
}

void KidsManagementApiMockSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_.ShutdownAndWaitUntilComplete());
}

}  // namespace supervised_user
