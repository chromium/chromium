// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/child_account_test_utils.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace supervised_user {

std::string GetChildAccountOAuthIdToken() {
  std::string encoded = base::Base64Encode(R"({ "services": ["uca"] })");
  return base::StringPrintf("dummy-header.%s.dummy-signature", encoded.c_str());
}

void AddHostResolverRule(base::CommandLine* command_line,
                         std::string_view host,
                         const net::test_server::EmbeddedTestServer& target) {
  CHECK(target.Started());

  std::string current_rules =
      command_line->GetSwitchValueASCII(network::switches::kHostResolverRules);
  std::string new_rule =
      base::JoinString({"MAP", host, target.host_port_pair().ToString()}, " ");

  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::JoinString({current_rules, new_rule}, ","));
}

}  // namespace supervised_user
