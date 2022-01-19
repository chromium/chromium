// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using access_code_cast::mojom::AddSinkResultCode;
using MockAddSinkCallback =
    base::MockCallback<media_router::AccessCodeCastHandler::AddSinkCallback>;
using ::testing::_;
using ::testing::InvokeWithoutArgs;

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

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

class AccessCodeCastHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AccessCodeCastHandlerTest()
      : mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(mock_time_task_runner_)),
        message_handler_(mock_cast_socket_service_.get()),
        session_tracker_(
            new CastSessionTracker(&dual_media_sink_service_,
                                   &message_handler_,
                                   mock_cast_socket_service_->task_runner())),
        mock_cast_media_sink_service_impl_(
            new MockCastMediaSinkServiceImpl(mock_sink_discovered_cb_.Get(),
                                             mock_cast_socket_service_.get(),
                                             discovery_network_monitor_.get(),
                                             &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            web_contents()->GetBrowserContext(),
            base::BindRepeating(&MockMediaRouter::Create)));
    media_router::CastModeSet cast_mode_set = {
        media_router::MediaCastMode::DESKTOP_MIRROR};
    handler_ = std::make_unique<AccessCodeCastHandler>(
        mojo::PendingReceiver<access_code_cast::mojom::PageHandler>(),
        page_.BindAndGetRemote(),
        profile_manager()->CreateTestingProfile("foo_email"), router_,
        cast_mode_set, web_contents(),
        mock_cast_media_sink_service_impl_.get());
  }
  void TearDown() override {
    handler_.reset();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }
  AccessCodeCastHandler* handler() { return handler_.get(); }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  raw_ptr<MockMediaRouter> router_;

  static std::vector<DiscoveryNetworkInfo> GetFakeNetworkInfo() {
    return {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
    ;
  }

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&GetFakeNetworkInfo);

  std::unique_ptr<AccessCodeCastHandler> handler_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;

  testing::NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  std::unique_ptr<media_router::CastSessionTracker> session_tracker_;
  testing::StrictMock<MockPage> page_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<MockCastMediaSinkServiceImpl>
      mock_cast_media_sink_service_impl_;
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

  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  handler()->HandleSinkPresentInMediaRouter(cast_sink1, true);
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
}  // namespace media_router
