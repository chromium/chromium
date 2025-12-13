// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"

namespace content::webid {

class IdentityProviderServiceTest : public PlatformBrowserTest {
 protected:
  IdentityProviderServiceTest() = default;
  ~IdentityProviderServiceTest() override = default;
};

IN_PROC_BROWSER_TEST_F(IdentityProviderServiceTest, FetchData) {
  auto idp_service = std::make_unique<IdentityProviderService>();
  base::test::TestFuture<bool> connected;
  idp_service->Connect("org.chromium.android_browsertests_apk",
                       "org.chromium.chrome.browser.webid.TestIdP",
                       connected.GetCallback());
  ASSERT_TRUE(connected.Get());

  base::test::TestFuture<const std::optional<std::string>&> response;
  idp_service->Fetch(response.GetCallback());
  ASSERT_TRUE(response.Get().has_value());
  ASSERT_EQ("Hello? Hello world!", response.Get().value());

  base::test::TestFuture<void> disconnected;
  idp_service->Disconnect(disconnected.GetCallback());
  ASSERT_TRUE(disconnected.Wait());
}

IN_PROC_BROWSER_TEST_F(IdentityProviderServiceTest, ConnectFails) {
  auto idp_service = std::make_unique<IdentityProviderService>();
  base::test::TestFuture<bool> connected;
  idp_service->Connect("does.not.exist", "neither.this",
                       connected.GetCallback());
  ASSERT_FALSE(connected.Get());
}

IN_PROC_BROWSER_TEST_F(IdentityProviderServiceTest,
                       ConnectsAfterDisconnecting) {
  auto idp_service = std::make_unique<IdentityProviderService>();

  base::test::TestFuture<bool> connected1;
  idp_service->Connect("org.chromium.android_browsertests_apk",
                       "org.chromium.chrome.browser.webid.TestIdP",
                       connected1.GetCallback());
  ASSERT_TRUE(connected1.Get());

  base::test::TestFuture<void> disconnected;
  idp_service->Disconnect(disconnected.GetCallback());
  ASSERT_TRUE(disconnected.Wait());

  base::test::TestFuture<bool> connected2;
  idp_service->Connect("org.chromium.android_browsertests_apk",
                       "org.chromium.chrome.browser.webid.TestIdP",
                       connected2.GetCallback());
  ASSERT_TRUE(connected2.Get());
}

IN_PROC_BROWSER_TEST_F(IdentityProviderServiceTest,
                       DisconnectsWhileDisconnected) {
  auto idp_service = std::make_unique<IdentityProviderService>();

  base::test::TestFuture<void> disconnected;
  idp_service->Disconnect(disconnected.GetCallback());
  ASSERT_TRUE(disconnected.Wait());
}

IN_PROC_BROWSER_TEST_F(IdentityProviderServiceTest, ConnectsTwice) {
  auto idp_service = std::make_unique<IdentityProviderService>();

  base::test::TestFuture<bool> connected1;
  idp_service->Connect("org.chromium.android_browsertests_apk",
                       "org.chromium.chrome.browser.webid.TestIdP",
                       connected1.GetCallback());
  ASSERT_TRUE(connected1.Get());

  base::test::TestFuture<bool> connected2;
  idp_service->Connect("org.chromium.android_browsertests_apk",
                       "org.chromium.chrome.browser.webid.TestIdP",
                       connected2.GetCallback());
  ASSERT_TRUE(connected2.Get());
}

}  // namespace content::webid
