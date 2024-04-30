// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_API_MOCK_SETUP_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_API_MOCK_SETUP_MIXIN_H_

#include <string_view>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace supervised_user {

// Manages an instance of embedded test server which is running the
// KidsManagementApiServerMock.
class KidsManagementApiMockSetupMixin : public InProcessBrowserTestMixin {
 public:
  static void SetApiHostname(std::string_view hostname);

  KidsManagementApiMockSetupMixin() = delete;
  KidsManagementApiMockSetupMixin(InProcessBrowserTestMixinHost& host,
                                  InProcessBrowserTest* test_base);
  KidsManagementApiMockSetupMixin(const KidsManagementApiMockSetupMixin&) =
      delete;
  KidsManagementApiMockSetupMixin& operator=(
      const KidsManagementApiMockSetupMixin&) = delete;
  ~KidsManagementApiMockSetupMixin() override;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  KidsManagementApiServerMock& api_mock() { return api_mock_; }

 private:
  raw_ptr<InProcessBrowserTest> test_base_;

  // Used exclusively to host KidsManagementApiServerMocks.
  net::EmbeddedTestServer embedded_test_server_;

  // Mocks server functionalities.
  KidsManagementApiServerMock api_mock_;

  // Hosts kSupervisedUserProtoFetcherConfig feature parameter value, which
  // alters the behavior of api clients (Kids Api Proto Fetchers) to connect to
  // the embedded test server which is running this mock.
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_API_MOCK_SETUP_MIXIN_H_
