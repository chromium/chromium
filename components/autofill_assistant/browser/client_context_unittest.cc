// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_context.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;

namespace {

class ClientContextTest : public testing::Test {
 protected:
  ClientContextTest() {}
  ~ClientContextTest() override {}

  NiceMock<MockClient> mock_client_;
  DeviceContext device_context_;
};

TEST_F(ClientContextTest, Initialize) {
  device_context_.version.sdk_int = 123;
  device_context_.manufacturer.assign("manufacturer");
  device_context_.model.assign("model");

  EXPECT_CALL(mock_client_, GetLocale()).WillOnce(Return("de-DE"));
  EXPECT_CALL(mock_client_, GetCountryCode()).WillOnce(Return("ZZ"));
  EXPECT_CALL(mock_client_, GetDeviceContext())
      .WillOnce(Return(device_context_));
  EXPECT_CALL(mock_client_, GetChromeSignedInEmailAddress())
      .WillOnce(Return("john.doe@chromium.org"));
  EXPECT_CALL(mock_client_, IsAccessibilityEnabled()).WillOnce(Return(true));

  ClientContextImpl client_context(&mock_client_);

  auto actual_client_context = client_context.AsProto();
  EXPECT_THAT(actual_client_context.chrome().chrome_version(),
              Eq(version_info::GetProductNameAndVersionForUserAgent()));
  EXPECT_THAT(actual_client_context.locale(), Eq("de-DE"));
  EXPECT_THAT(actual_client_context.country(), Eq("ZZ"));
  EXPECT_THAT(actual_client_context.experiment_ids(), Eq(""));
  EXPECT_THAT(actual_client_context.is_cct(), Eq(false));
  EXPECT_THAT(actual_client_context.is_onboarding_shown(), Eq(false));
  EXPECT_THAT(actual_client_context.is_direct_action(), Eq(false));
  EXPECT_THAT(actual_client_context.accessibility_enabled(), Eq(true));
  EXPECT_THAT(actual_client_context.signed_into_chrome_status(),
              Eq(ClientContextProto::SIGNED_IN));
  EXPECT_THAT(actual_client_context.accounts_matching_status(),
              Eq(ClientContextProto::UNKNOWN));

  auto actual_device_context = actual_client_context.device_context();
  EXPECT_THAT(actual_device_context.version().sdk_int(), Eq(123));
  EXPECT_THAT(actual_device_context.manufacturer(), Eq("manufacturer"));
  EXPECT_THAT(actual_device_context.model(), Eq("model"));
}

TEST_F(ClientContextTest, UpdateWithTriggerContext) {
  EXPECT_CALL(mock_client_, GetChromeSignedInEmailAddress())
      .WillRepeatedly(Return(""));
  EXPECT_CALL(mock_client_, IsAccessibilityEnabled())
      .WillRepeatedly(Return(true));

  ClientContextImpl client_context(&mock_client_);
  client_context.Update({/* params = */ {},
                         /* exp = */ "1,2,3",
                         /* is_cct = */ true,
                         /* onboarding_shown = */ true,
                         /* is_direct_action = */ true,
                         /* caller_account_hash = */ "some_value"});

  auto actual_client_context = client_context.AsProto();
  EXPECT_THAT(actual_client_context.experiment_ids(), Eq("1,2,3"));
  EXPECT_THAT(actual_client_context.is_cct(), Eq(true));
  EXPECT_THAT(actual_client_context.is_onboarding_shown(), Eq(true));
  EXPECT_THAT(actual_client_context.is_direct_action(), Eq(true));
  EXPECT_THAT(actual_client_context.accessibility_enabled(), Eq(true));
  EXPECT_THAT(actual_client_context.experiment_ids(), Eq("1,2,3"));
  EXPECT_THAT(actual_client_context.signed_into_chrome_status(),
              Eq(ClientContextProto::NOT_SIGNED_IN));
  EXPECT_THAT(actual_client_context.accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_NOT_MATCHING));
}

TEST_F(ClientContextTest, AccountMatching) {
  EXPECT_CALL(mock_client_, GetChromeSignedInEmailAddress())
      .WillRepeatedly(Return("john.doe@chromium.org"));

  ClientContextImpl client_context(&mock_client_);
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::UNKNOWN));

  // Should match john.doe@chromium.org.
  client_context.Update(
      {/* params = */ {},
       /* exp = */ std::string(),
       /* is_cct = */ false,
       /* onboarding_shown = */ false,
       /* is_direct_action = */ false,
       /* caller_account_hash = */
       "2c8fa87717fab622bb5cc4d18135fe30dae339efd274b450022d361be92b48c3"});
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_MATCHING));

  client_context.Update({/* params = */ {},
                         /* exp = */ std::string(),
                         /* is_cct = */ false,
                         /* onboarding_shown = */ false,
                         /* is_direct_action = */ false,
                         /* caller_account_hash = */
                         "different"});
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_NOT_MATCHING));
}

TEST_F(ClientContextTest, SignedInStatus) {
  EXPECT_CALL(mock_client_, GetChromeSignedInEmailAddress())
      .WillOnce(Return(""));

  ClientContextImpl client_context_a(&mock_client_);
  EXPECT_THAT(client_context_a.AsProto().signed_into_chrome_status(),
              Eq(ClientContextProto::NOT_SIGNED_IN));

  EXPECT_CALL(mock_client_, GetChromeSignedInEmailAddress())
      .WillOnce(Return("john.doe@chromium.org"));
  ClientContextImpl client_context_b(&mock_client_);
  EXPECT_THAT(client_context_b.AsProto().signed_into_chrome_status(),
              Eq(ClientContextProto::SIGNED_IN));
}

}  // namespace
}  // namespace autofill_assistant
