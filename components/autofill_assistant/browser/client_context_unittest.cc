// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_context.h"

#include "base/containers/flat_map.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

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
  EXPECT_CALL(mock_client_, GetWindowSize())
      .WillOnce(Return(std::make_pair(1080, 1920)));
  EXPECT_CALL(mock_client_, GetScreenOrientation())
      .WillOnce(Return(ClientContextProto::PORTRAIT));
  EXPECT_CALL(mock_client_, GetSignedInEmail())
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
  EXPECT_THAT(actual_client_context.window_size().width_pixels(), Eq(1080));
  EXPECT_THAT(actual_client_context.window_size().height_pixels(), Eq(1920));
  EXPECT_THAT(actual_client_context.screen_orientation(),
              ClientContextProto::PORTRAIT);
  EXPECT_EQ(actual_client_context.js_flow_library_loaded(), false);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(actual_client_context.platform_type(),
              ClientContextProto::PLATFORM_TYPE_ANDROID);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_THAT(actual_client_context.platform_type(),
              ClientContextProto::PLATFORM_TYPE_DESKTOP);
#endif

  auto actual_device_context = actual_client_context.device_context();
  EXPECT_THAT(actual_device_context.version().sdk_int(), Eq(123));
  EXPECT_THAT(actual_device_context.manufacturer(), Eq("manufacturer"));
  EXPECT_THAT(actual_device_context.model(), Eq("model"));
}

TEST_F(ClientContextTest, UpdatesToClientContext) {
  // Calls expected when the constructor is called.
  EXPECT_CALL(mock_client_, IsAccessibilityEnabled()).WillOnce(Return(false));
  EXPECT_CALL(mock_client_, GetSignedInEmail())
      .WillOnce(Return("john.doe@chromium.org"));
  EXPECT_CALL(mock_client_, GetWindowSize())
      .WillOnce(Return(std::make_pair(0, 0)));
  EXPECT_CALL(mock_client_, GetScreenOrientation())
      .WillOnce(Return(ClientContextProto::PORTRAIT));
  ClientContextImpl client_context(&mock_client_);

  // Calls expected when Update is called. We expect the previous entries to
  // be overwritten.
  EXPECT_CALL(mock_client_, IsAccessibilityEnabled()).WillOnce(Return(true));
  EXPECT_CALL(mock_client_, GetSignedInEmail()).WillOnce(Return(""));
  EXPECT_CALL(mock_client_, GetWindowSize())
      .WillOnce(Return(std::pair<int, int>(1080, 1920)));
  EXPECT_CALL(mock_client_, GetScreenOrientation())
      .WillOnce(Return(ClientContextProto::LANDSCAPE));

  client_context.Update({std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"USER_EMAIL", "example@chromium.org"}}),
                         /* experiment_ids = */ "1,2,3",
                         /* is_cct = */ true,
                         /* onboarding_shown = */ true,
                         /* is_direct_action = */ true,
                         /* initial_url = */ "https://www.example.com",
                         /* is_in_chrome_triggered = */ true});
  auto actual_client_context = client_context.AsProto();
  EXPECT_THAT(actual_client_context.experiment_ids(), Eq("1,2,3"));
  EXPECT_THAT(actual_client_context.is_cct(), Eq(true));
  EXPECT_THAT(actual_client_context.is_onboarding_shown(), Eq(true));
  EXPECT_THAT(actual_client_context.is_direct_action(), Eq(true));
  EXPECT_THAT(actual_client_context.is_in_chrome_triggered(), Eq(true));
  EXPECT_THAT(actual_client_context.accessibility_enabled(), Eq(true));
  EXPECT_THAT(actual_client_context.experiment_ids(), Eq("1,2,3"));
  EXPECT_THAT(actual_client_context.signed_into_chrome_status(),
              Eq(ClientContextProto::NOT_SIGNED_IN));
  EXPECT_THAT(actual_client_context.accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_NOT_MATCHING));
  EXPECT_THAT(actual_client_context.window_size().width_pixels(), Eq(1080));
  EXPECT_THAT(actual_client_context.window_size().height_pixels(), Eq(1920));
  EXPECT_THAT(actual_client_context.screen_orientation(),
              ClientContextProto::LANDSCAPE);
  EXPECT_FALSE(actual_client_context.has_annotate_dom_model_context());

  client_context.UpdateAnnotateDomModelContext(123456);
  actual_client_context = client_context.AsProto();
  EXPECT_THAT(
      actual_client_context.annotate_dom_model_context().model_version(),
      123456);
}

TEST_F(ClientContextTest, WindowSizeIsClearedIfNoLongerAvailable) {
  // When we call the context's constructor, we have a window size.
  EXPECT_CALL(mock_client_, GetWindowSize())
      .WillOnce(Return(std::make_pair(1080, 1920)));
  ClientContextImpl client_context(&mock_client_);

  // When we update the context, there is no window size anymore.
  EXPECT_CALL(mock_client_, GetWindowSize()).WillOnce(Return(absl::nullopt));
  auto actual_client_context = client_context.AsProto();
  EXPECT_THAT(actual_client_context.window_size().width_pixels(), Eq(1080));
  EXPECT_THAT(actual_client_context.window_size().height_pixels(), Eq(1920));

  client_context.Update({std::make_unique<ScriptParameters>(),
                         /* exp = */ "1,2,3",
                         /* is_cct = */ true,
                         /* onboarding_shown = */ true,
                         /* is_direct_action = */ true,
                         /* initial_url = */ "https://www.example.com",
                         /* is_in_chrome_triggered = */ false});

  actual_client_context = client_context.AsProto();
  EXPECT_FALSE(actual_client_context.has_window_size());
}

TEST_F(ClientContextTest, AccountMatching) {
  EXPECT_CALL(mock_client_, GetSignedInEmail())
      .WillRepeatedly(Return("john.doe@chromium.org"));

  ClientContextImpl client_context(&mock_client_);
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::UNKNOWN));

  client_context.Update({std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"USER_EMAIL", "john.doe@chromium.org"}}),
                         /* exp = */ std::string(),
                         /* is_cct = */ false,
                         /* onboarding_shown = */ false,
                         /* is_direct_action = */ false,
                         /* initial_url = */ "https://www.example.com",
                         /* is_in_chrome_triggered = */ false});
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_MATCHING));

  client_context.Update({std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"USER_EMAIL", "lisa.doe@chromium.org"}}),
                         /* exp = */ std::string(),
                         /* is_cct = */ false,
                         /* onboarding_shown = */ false,
                         /* is_direct_action = */ false,
                         /* initial_url = */ "https://www.example.com",
                         /* is_in_chrome_triggered = */ false});
  EXPECT_THAT(client_context.AsProto().accounts_matching_status(),
              Eq(ClientContextProto::ACCOUNTS_NOT_MATCHING));
}

TEST_F(ClientContextTest, SignedInStatus) {
  EXPECT_CALL(mock_client_, GetSignedInEmail()).WillOnce(Return(""));

  ClientContextImpl client_context_a(&mock_client_);
  EXPECT_THAT(client_context_a.AsProto().signed_into_chrome_status(),
              Eq(ClientContextProto::NOT_SIGNED_IN));

  EXPECT_CALL(mock_client_, GetSignedInEmail())
      .WillOnce(Return("john.doe@chromium.org"));
  ClientContextImpl client_context_b(&mock_client_);
  EXPECT_THAT(client_context_b.AsProto().signed_into_chrome_status(),
              Eq(ClientContextProto::SIGNED_IN));
}

TEST_F(ClientContextTest, UpdateJsFlowLibraryLoaded) {
  ClientContextImpl client_context(&mock_client_);
  EXPECT_EQ(client_context.AsProto().js_flow_library_loaded(), false);
  client_context.UpdateJsFlowLibraryLoaded(true);
  EXPECT_EQ(client_context.AsProto().js_flow_library_loaded(), true);
  client_context.UpdateJsFlowLibraryLoaded(false);
  EXPECT_EQ(client_context.AsProto().js_flow_library_loaded(), false);
}

}  // namespace
}  // namespace autofill_assistant
