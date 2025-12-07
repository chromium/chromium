// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_device_list_host.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/test/mock_media_dialog_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_router::CastDialogModel;
using media_router::UIMediaSink;
using media_router::UIMediaSinkState;
using testing::_;
using testing::NiceMock;
using testing::Return;
namespace mojom {
using global_media_controls::mojom::DeviceListClient;
}  // namespace mojom

namespace {

constexpr char kSinkId[] = "sink_id";
constexpr char16_t kSinkFriendlyName16[] = u"Device Name";

UIMediaSink CreateMediaSink(
    UIMediaSinkState state = UIMediaSinkState::AVAILABLE) {
  UIMediaSink sink{media_router::mojom::MediaRouteProviderId::CAST};
  sink.friendly_name = kSinkFriendlyName16;
  sink.id = kSinkId;
  sink.state = state;
  sink.cast_modes = {media_router::MediaCastMode::PRESENTATION};
  return sink;
}

CastDialogModel CreateModelWithSinks(std::vector<UIMediaSink> sinks) {
  CastDialogModel model;
  model.set_media_sinks(std::move(sinks));
  return model;
}

class MockCastDialogController : public media_router::CastDialogController {
 public:
  MOCK_METHOD(void, AddObserver, (CastDialogController::Observer * observer));
  MOCK_METHOD(void,
              RemoveObserver,
              (CastDialogController::Observer * observer));
  MOCK_METHOD(void,
              StartCasting,
              (const std::string& sink_id,
               media_router::MediaCastMode cast_mode));
  MOCK_METHOD(void, StopCasting, (const std::string& route_id));
  MOCK_METHOD(void, ClearIssue, (const media_router::Issue::Id& issue_id));
  MOCK_METHOD(void, FreezeRoute, (const std::string& route_id));
  MOCK_METHOD(void, UnfreezeRoute, (const std::string& route_id));
  MOCK_METHOD(std::unique_ptr<media_router::MediaRouteStarter>,
              TakeMediaRouteStarter,
              ());
  MOCK_METHOD(void, RegisterDestructor, (base::OnceClosure));
};

}  // namespace

class CastDeviceListHostTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    auto dialog_controller = std::make_unique<MockCastDialogController>();
    dialog_controller_ = dialog_controller.get();
    host_ = CreateHost(std::move(dialog_controller), mock_client_.PassRemote());
  }

  MOCK_METHOD(void, OnMediaRemotingRequested, ());
  MOCK_METHOD(void, HideMediaDialog, ());
  MOCK_METHOD(void, OnSinksDiscoveredCallback, ());

  const global_media_controls::test::MockDeviceListClient& mock_client() {
    return mock_client_;
  }

  void FlushForTesting() { mock_client_.FlushForTesting(); }

 protected:
  std::unique_ptr<CastDeviceListHost> CreateHost(
      std::unique_ptr<media_router::CastDialogController> dialog_controller,
      mojo::PendingRemote<mojom::DeviceListClient> cleint_remote) {
    return std::make_unique<CastDeviceListHost>(
        std::move(dialog_controller), std::move(cleint_remote),
        base::BindRepeating(&CastDeviceListHostTest::OnMediaRemotingRequested,
                            base::Unretained(this)),
        base::BindRepeating(&CastDeviceListHostTest::HideMediaDialog,
                            base::Unretained(this)),
        base::BindRepeating(&CastDeviceListHostTest::OnSinksDiscoveredCallback,
                            base::Unretained(this)));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CastDeviceListHost> host_;
  raw_ptr<MockCastDialogController> dialog_controller_ = nullptr;
  global_media_controls::test::MockDeviceListClient mock_client_;
};

TEST_F(CastDeviceListHostTest, StartPresentation) {
  UIMediaSink available_sink = CreateMediaSink();
  UIMediaSink connected_sink = CreateMediaSink(UIMediaSinkState::CONNECTED);
  connected_sink.id = "connected_sink";
  connected_sink.provider = media_router::mojom::MediaRouteProviderId::CAST;
  host_->OnModelUpdated(CreateModelWithSinks({available_sink, connected_sink}));

  // Selecting available or connected CAST sinks will start casting.
  EXPECT_CALL(*dialog_controller_,
              StartCasting(available_sink.id,
                           media_router::MediaCastMode::PRESENTATION));
  host_->SelectDevice(available_sink.id);
  testing::Mock::VerifyAndClearExpectations(dialog_controller_);
  EXPECT_CALL(*dialog_controller_,
              StartCasting(connected_sink.id,
                           media_router::MediaCastMode::PRESENTATION));
  host_->SelectDevice(connected_sink.id);
}

TEST_F(CastDeviceListHostTest, NoopOnConnectingOrDisconnectingSink) {
  UIMediaSink connecting_sink = CreateMediaSink(UIMediaSinkState::CONNECTING);
  UIMediaSink disconnecting_sink =
      CreateMediaSink(UIMediaSinkState::DISCONNECTING);
  disconnecting_sink.id = "id2";
  host_->OnModelUpdated(
      CreateModelWithSinks({connecting_sink, disconnecting_sink}));

  // Selecting a connecting or disconnecting sink will not start casting.
  EXPECT_CALL(*dialog_controller_, StartCasting(_, _)).Times(0);
  host_->SelectDevice(connecting_sink.id);
  host_->SelectDevice(disconnecting_sink.id);
}

TEST_F(CastDeviceListHostTest, StartRemotePlayback) {
  UIMediaSink sink = CreateMediaSink();
  sink.cast_modes = {media_router::MediaCastMode::REMOTE_PLAYBACK};
  host_->OnModelUpdated({CreateModelWithSinks({sink})});
  EXPECT_CALL(
      *dialog_controller_,
      StartCasting(sink.id, media_router::MediaCastMode::REMOTE_PLAYBACK));
  EXPECT_CALL(*this, OnMediaRemotingRequested());
  host_->SelectDevice(sink.id);
}

// TODO(crbug.com/1486680): Enable this on Chrome OS once stopping mirroring
// routes in the global media controls is implemented.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(CastDeviceListHostTest, StartAudioTabMirroring) {
  auto sink = CreateMediaSink();
  sink.cast_modes = {media_router::MediaCastMode::TAB_MIRROR};
  sink.icon_type = media_router::SinkIconType::CAST_AUDIO;
  host_->OnModelUpdated({CreateModelWithSinks({sink})});

  EXPECT_CALL(*dialog_controller_,
              StartCasting(sink.id, media_router::MediaCastMode::TAB_MIRROR));
  host_->SelectDevice(sink.id);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(CastDeviceListHostTest, OnSinksDiscovered) {
  EXPECT_CALL(*this, OnSinksDiscoveredCallback());
  EXPECT_CALL(mock_client(), OnDevicesUpdated);
  UIMediaSink sink = CreateMediaSink();
  sink.cast_modes = {media_router::MediaCastMode::REMOTE_PLAYBACK};
  host_->OnModelUpdated({CreateModelWithSinks({sink})});
  FlushForTesting();

  EXPECT_CALL(*this, OnSinksDiscoveredCallback()).Times(0);
  EXPECT_CALL(mock_client(), OnDevicesUpdated);
  host_->OnModelUpdated({CreateModelWithSinks({})});
  FlushForTesting();
}

TEST_F(CastDeviceListHostTest, OnDiscoveryPermissionRejected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media_router::kShowCastPermissionRejectedError);

  EXPECT_CALL(*this, OnSinksDiscoveredCallback()).Times(0);
  EXPECT_CALL(mock_client(), OnPermissionRejected);
  CastDialogModel model = CreateModelWithSinks({CreateMediaSink()});
  model.set_is_permission_rejected(true);
  host_->OnModelUpdated(model);
  FlushForTesting();
}

TEST_F(CastDeviceListHostTest, HideMediaDialogCallback) {
  EXPECT_CALL(*this, HideMediaDialog());
  host_->OnCastingStarted();
}

TEST_F(CastDeviceListHostTest, TerminateDialSession) {
  auto dial_connected_sink = CreateMediaSink(UIMediaSinkState::CONNECTED);
  dial_connected_sink.provider =
      media_router::mojom::MediaRouteProviderId::DIAL;
  dial_connected_sink.route =
      media_router::MediaRoute("routeId1", media_router::MediaSource("source1"),
                               "sinkId1", "description", true);
  host_->OnModelUpdated(CreateModelWithSinks({dial_connected_sink}));

  // Selecting connected DIAL sinks will terminate casting.
  // TODO(crbug.com/1206830): change test cases after DIAL MRP supports
  // launching a session on a connected sink.
  EXPECT_CALL(*dialog_controller_, StopCasting("routeId1"));
  host_->SelectDevice(dial_connected_sink.id);
}

TEST_F(CastDeviceListHostTest, SelectingDeviceClearsIssue) {
  auto sink = CreateMediaSink();
  media_router::IssueInfo issue_info(
      "Issue Title", media_router::IssueInfo::Severity::WARNING, sink.id);
  media_router::Issue issue(
      media_router::Issue::CreateIssueWithIssueInfo(issue_info));
  sink.issue = issue;
  host_->OnModelUpdated(CreateModelWithSinks({sink}));

  // Selecting sinks with issue will clear up the issue instead of starting a
  // cast session.
  EXPECT_CALL(*dialog_controller_, StartCasting(_, _)).Times(0);
  EXPECT_CALL(*dialog_controller_, ClearIssue(issue.id()));
  host_->SelectDevice(sink.id);
}

TEST_F(CastDeviceListHostTest, GetId) {
  mojo::PendingReceiver<mojom::DeviceListClient> client_receiver;
  std::unique_ptr<CastDeviceListHost> host2 =
      CreateHost(std::make_unique<MockCastDialogController>(),
                 client_receiver.InitWithNewPipeAndPassRemote());
  // IDs should be unique.
  EXPECT_NE(host_->id(), host2->id());
}
