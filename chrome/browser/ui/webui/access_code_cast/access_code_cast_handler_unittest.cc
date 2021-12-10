// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using access_code_cast::mojom::AddSinkResultCode;
using MockAddSinkCallback =
    base::MockCallback<AccessCodeCastHandler::AddSinkCallback>;
using ::testing::_;

namespace {
class MockPage : public access_code_cast::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<access_code_cast::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<access_code_cast::mojom::Page> receiver_{this};
};
}  // namespace

class AccessCodeCastHandlerTest : public testing::Test {
 protected:
  AccessCodeCastHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    handler_ = std::make_unique<AccessCodeCastHandler>(
        mojo::PendingReceiver<access_code_cast::mojom::PageHandler>(),
        page_.BindAndGetRemote(),
        profile_manager()->CreateTestingProfile("foo_email"));
  }
  void TearDown() override { handler_.reset(); }
  AccessCodeCastHandler* handler() { return handler_.get(); }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  std::unique_ptr<AccessCodeCastHandler> handler_;
  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;
  testing::StrictMock<MockPage> page_;
  TestingProfileManager profile_manager_;
};

TEST_F(AccessCodeCastHandlerTest, DiscoveryDeviceMissingWithOk) {
  // Test to ensure that the add_sink_callback returns an EMPTY_RESPONSE if the
  // the device is missing. Since |OnAccessCodeValidated| is a public method --
  // we must check the case of an empty |discovery_device| with an OK result
  // code.
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::EMPTY_RESPONSE));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(absl::nullopt, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastHandlerTest, ValidDiscoveryDeviceAndCode) {
  // If discovery device is present, formatted correctly, and code is OK, then
  // callback should be OK.
  MockAddSinkCallback mock_callback;
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto();

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(discovery_device_proto,
                                   AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastHandlerTest, InvalidDiscoveryDevice) {
  // If discovery device is present, but formatted incorrectly, and code is OK,
  // then callback should be SINK_CREATION_ERROR.
  MockAddSinkCallback mock_callback;

  // Create discovery_device with an invalid port
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto("foo_display_name", "1234",
                                              "```````23489:1238:1239");

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::SINK_CREATION_ERROR));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(discovery_device_proto,
                                   AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastHandlerTest, NonOKResultCode) {
  // Check to see that any result code that isn't OK will return that error.
  MockAddSinkCallback mock_callback;

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::AUTH_ERROR));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(absl::nullopt,
                                   AddSinkResultCode::AUTH_ERROR);
}
