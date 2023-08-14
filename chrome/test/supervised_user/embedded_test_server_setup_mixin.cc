// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"

#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace supervised_user {

namespace {

constexpr base::StringPiece kKidsManagementServiceEndpoint{
    "kidsmanagement.googleapis.com"};

std::string CreateResolverRule(base::StringPiece host,
                               base::StringPiece target) {
  return base::StrCat({"MAP ", host, " ", target});
}

std::vector<std::string> SplitHostList(base::StringPiece host_list) {
  return base::SplitString(host_list, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

inline void AddKidsManagementHostToResolve(
    std::vector<std::string>& resolver_list) {
  resolver_list.emplace_back(kKidsManagementServiceEndpoint);
}
}  // namespace

EmbeddedTestServerSetupMixin::EmbeddedTestServerSetupMixin(
    InProcessBrowserTestMixinHost& host,
    raw_ptr<net::EmbeddedTestServer> server,
    const Options& options)
    : InProcessBrowserTestMixin(&host),
      embedded_test_server_(server),
      resolver_rules_map_host_list_(
          SplitHostList(options.resolver_rules_map_host_list)) {
  CHECK(server) << "This mixin requires an embedded test server";
  AddKidsManagementHostToResolve(resolver_rules_map_host_list_);
}

EmbeddedTestServerSetupMixin::~EmbeddedTestServerSetupMixin() = default;

void EmbeddedTestServerSetupMixin::SetUp() {
  api_mock_.InstallOn(embedded_test_server_);
  CHECK(embedded_test_server_->InitializeAndListen());
  LOG(INFO) << "Kids management server is up and running on "
            << embedded_test_server_->host_port_pair().ToString();
}

void EmbeddedTestServerSetupMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  CHECK(embedded_test_server_->Started());

  std::string target = embedded_test_server_->host_port_pair().ToString();
  std::vector<std::string> resolver_rules(resolver_rules_map_host_list_.size());

  base::ranges::transform(resolver_rules_map_host_list_, resolver_rules.begin(),
                          [&](const std::string& host) -> std::string {
                            return CreateResolverRule(host, target);
                          });

  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::JoinString(base::span<std::string>(resolver_rules), ", "));
  LOG(INFO) << "Following hosts will be mapped to kids management server: ";
  for (const std::string& rule : resolver_rules) {
    LOG(INFO) << "\t" << rule;
  }
}

void EmbeddedTestServerSetupMixin::SetUpOnMainThread() {
  embedded_test_server_->StartAcceptingConnections();
}

void EmbeddedTestServerSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_->ShutdownAndWaitUntilComplete());
}

KidsManagementApiServerMock& EmbeddedTestServerSetupMixin::GetApiMock() {
  return api_mock_;
}

void EmbeddedTestServerSetupMixin::InitFeatures() {
  SetHttpEndpointsForKidsManagementApis(feature_list_,
                                        kKidsManagementServiceEndpoint);
}
}  // namespace supervised_user
