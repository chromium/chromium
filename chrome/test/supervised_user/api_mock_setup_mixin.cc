// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/test/supervised_user/api_mock_setup_mixin.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"

namespace supervised_user {

namespace {
constexpr base::StringPiece kKidsManagementServiceEndpoint{
    "kidsmanagement.googleapis.com"};
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
  embedded_test_server_.StartAcceptingConnections();
}

void KidsManagementApiMockSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_.ShutdownAndWaitUntilComplete());
}

}  // namespace supervised_user
