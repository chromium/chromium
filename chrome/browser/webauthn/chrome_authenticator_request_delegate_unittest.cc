// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(ChromeAuthenticatorRequestDelegateTest, TestTransportPrefType) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  EXPECT_FALSE(delegate.GetLastTransportUsed());
  delegate.UpdateLastTransportUsed(device::FidoTransportProtocol::kInternal);
  const auto transport = delegate.GetLastTransportUsed();
  ASSERT_TRUE(transport);
  EXPECT_EQ(device::FidoTransportProtocol::kInternal, transport);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TestPairedDeviceAddressPreference) {
  static constexpr char kTestPairedDeviceAddress[] = "paired_device_address";
  static constexpr char kTestPairedDeviceAddress2[] = "paired_device_address2";

  ChromeAuthenticatorRequestDelegate delegate(main_rfh());

  auto* const address_list =
      delegate.GetPreviouslyPairedFidoBleDeviceAddresses();
  ASSERT_TRUE(address_list);
  EXPECT_TRUE(address_list->empty());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress);
  const auto* updated_address_list =
      delegate.GetPreviouslyPairedFidoBleDeviceAddresses();
  ASSERT_TRUE(updated_address_list);
  ASSERT_EQ(1u, updated_address_list->GetSize());

  const auto& address_value = updated_address_list->GetList().at(0);
  ASSERT_TRUE(address_value.is_string());
  EXPECT_EQ(kTestPairedDeviceAddress, address_value.GetString());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress);
  const auto* address_list_with_duplicate_address_added =
      delegate.GetPreviouslyPairedFidoBleDeviceAddresses();
  ASSERT_TRUE(address_list_with_duplicate_address_added);
  EXPECT_EQ(1u, address_list_with_duplicate_address_added->GetSize());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress2);
  const auto* address_list_with_two_addresses =
      delegate.GetPreviouslyPairedFidoBleDeviceAddresses();
  ASSERT_TRUE(address_list_with_two_addresses);

  ASSERT_EQ(2u, address_list_with_two_addresses->GetSize());
  const auto& second_address_value =
      address_list_with_two_addresses->GetList().at(1);
  ASSERT_TRUE(second_address_value.is_string());
  EXPECT_EQ(kTestPairedDeviceAddress2, second_address_value.GetString());
}

#if defined(OS_MACOSX)
std::string TouchIdMetadataSecret(
    const ChromeAuthenticatorRequestDelegate& delegate) {
  base::Optional<
      content::AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
      config = delegate.GetTouchIdAuthenticatorConfig();
  return config->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  std::string secret = TouchIdMetadataSecret(delegate);
  EXPECT_EQ(secret.size(), 32u);
  EXPECT_EQ(secret, TouchIdMetadataSecret(delegate));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return the
  // same secret.
  EXPECT_EQ(
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())),
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets. (No way to reset
  // browser_context(), so we have to create our own.)
  auto browser_context = base::WrapUnique(CreateBrowserContext());
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context.get(), nullptr);
  EXPECT_NE(
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())),
      TouchIdMetadataSecret(
          ChromeAuthenticatorRequestDelegate(web_contents->GetMainFrame())));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(32u, TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(
                                           web_contents->GetMainFrame()))
                     .size());
}
#endif

