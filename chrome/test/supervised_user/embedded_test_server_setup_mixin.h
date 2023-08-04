// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace supervised_user {

// An InProcessBrowserTestMixin that sets up an embedded test server (manages
// start and stop procedures and configures host resolver).
class EmbeddedTestServerSetupMixin : public InProcessBrowserTestMixin {
 public:
  // Use options class pattern to avoid growing list of arguments and take
  // advantage of auto-generated default constructor.
  struct Options {
    // Comma-separated list of hosts that will be mapped to server's address.
    // For example: if resolver_rules_map_host_list is
    // "example.com, *.another-example.com" and the embedded test server is
    // running at 127.0.0.1:3145, then the following resolver rule will be added
    // to chrome's commandline:
    // --host-resolver-rules=
    // 'MAP example.com 127.0.0.1:3145,  MAP *.another-example.com
    // 127.0.0.1:3145'.
    //
    // Internally, a host name for kids management api server mock is also
    // resolved to the associated embedded test server.
    std::string resolver_rules_map_host_list;
  };

  EmbeddedTestServerSetupMixin() = delete;
  explicit EmbeddedTestServerSetupMixin(InProcessBrowserTestMixinHost& host);
  EmbeddedTestServerSetupMixin(InProcessBrowserTestMixinHost& host,
                               base::raw_ptr<net::EmbeddedTestServer> server,
                               const Options& options);

  EmbeddedTestServerSetupMixin(const EmbeddedTestServerSetupMixin&) = delete;
  EmbeddedTestServerSetupMixin& operator=(const EmbeddedTestServerSetupMixin&) =
      delete;

  ~EmbeddedTestServerSetupMixin() override;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;
  void SetUpOnMainThread() override;

  // See SupervisionMixin::InitFeatures.
  void InitFeatures();

 private:
  // Embedded test server owned by test that uses this mixin.
  base::raw_ptr<net::EmbeddedTestServer> embedded_test_server_;

  // Mocks server functionalities.
  KidsManagementApiServerMock api_mock_;

  // List of hosts that will be resolved to server's address.
  std::vector<std::string> resolver_rules_map_host_list_;

  // Set and activated in ::InitFeatures.
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_
