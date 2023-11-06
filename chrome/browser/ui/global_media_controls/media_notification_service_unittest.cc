// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/cast_device_list_host.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/test/mock_media_dialog_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"
#endif

namespace mojom {
using global_media_controls::mojom::DeviceListClient;
using global_media_controls::mojom::DeviceListHost;
using global_media_controls::mojom::DevicePtr;
}  // namespace mojom

using global_media_controls::test::MockDeviceListClient;
using media_router::MediaRoute;
using media_router::StartPresentationContext;
using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;
using testing::AtLeast;
using testing::Expectation;
using testing::NiceMock;
using testing::Return;

namespace {

}  // namespace

class MediaNotificationServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaNotificationServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::MainThreadType::UI) {}
  ~MediaNotificationServiceTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    crosapi_environment_.SetUp();
#endif
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));
    service_ = std::make_unique<MediaNotificationService>(profile(), false);
  }

  void TearDown() override {
    SimulateCloseDialog();
    service_.reset();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    crosapi_environment_.TearDown();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::UnguessableToken SimulatePlayingControllableMedia(
      base::UnguessableToken id) {
    SimulateFocusGained(id, true);
    SimulateNecessaryMetadata(id);
    return id;
  }

  base::UnguessableToken SimulatePlayingControllableMediaForWebContents(
      content::WebContents* contents) {
    content::MediaSession::Get(contents);
    auto id = content::MediaSession::GetRequestIdFromWebContents(contents);
    SimulatePlayingControllableMedia(id);
    base::RunLoop().RunUntilIdle();
    return id;
  }

  AudioFocusRequestStatePtr CreateFocusRequest(const base::UnguessableToken& id,
                                               bool controllable) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = controllable;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);
    return focus;
  }

  void SimulateFocusGained(const base::UnguessableToken& id,
                           bool controllable) {
    service_->media_session_item_producer_->OnFocusGained(
        CreateFocusRequest(id, controllable));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    service_->media_session_item_producer_->OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the
    // MediaNotificationService to show a media session, that session needs
    // a title and artist. Typically this would happen through the media session
    // service, but since the service doesn't run for this test, we'll manually
    // grab the MediaNotificationItem from the MediaNotificationService and
    // set the metadata.
    auto* session_item = GetNotificationSessionItem(id);
    ASSERT_NE(nullptr, session_item);

    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    session_item->MediaSessionMetadataChanged(std::move(metadata));
  }

  void SimulateDialogOpened(
      global_media_controls::test::MockMediaDialogDelegate* delegate) {
    service_->media_item_manager()->SetDialogDelegate(delegate);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateCloseDialog() {
    service_->media_item_manager()->SetDialogDelegate(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateDialogOpenedForPresentationRequest(
      global_media_controls::test::MockMediaDialogDelegate* delegate,
      content::WebContents* content) {
    service_->SetDialogDelegateForWebContents(delegate, content);
  }

  void SimulateMediaRoutesUpdate(
      const std::vector<media_router::MediaRoute>& routes) {
    service_->cast_notification_producer_->OnRoutesUpdated(routes);
  }

  global_media_controls::MediaSessionNotificationItem*
  GetNotificationSessionItem(const base::UnguessableToken& id) {
    return static_cast<global_media_controls::MediaSessionNotificationItem*>(
        service_->media_session_item_producer_->GetMediaItem(id.ToString())
            .get());
  }

  MediaNotificationService* service() { return service_.get(); }

 private:
  std::unique_ptr<MediaNotificationService> service_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::TestCrosapiEnvironment crosapi_environment_;
#endif
};

// This class enables the features for starting/stopping cast sessions from
// the Zenith dialog.
// Also, it sets up the MockWebContentsPresentationManager as a test instance
// that's used by the MediaNotificationService to get MediaRoute updates.
class MediaNotificationServiceCastTest : public MediaNotificationServiceTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    media_router::WebContentsPresentationManager::SetTestInstance(
        presentation_manager_.get());
    MediaNotificationServiceTest::SetUp();
  }

  void TearDown() override {
    media_router::WebContentsPresentationManager::SetTestInstance(nullptr);
    MediaNotificationServiceTest::TearDown();
  }

  media_router::MediaRoute CreateMediaRoute(
      media_router::MediaRoute::Id route_id) {
    media_router::MediaRoute media_route(
        route_id, media_router::MediaSource("cast:123456"), "sink_id",
        "description", true);
    media_route.set_controller_type(
        media_router::RouteControllerType::kGeneric);
    return media_route;
  }

  content::PresentationRequest CreatePresentationRequest() {
    return content::PresentationRequest(main_rfh()->GetGlobalId(),
                                        {GURL(), GURL()},
                                        url::Origin::Create(GURL()));
  }

  std::unique_ptr<StartPresentationContext> CreateStartPresentationContext(
      content::PresentationRequest presentation_request,
      StartPresentationContext::PresentationConnectionCallback success_cb =
          base::DoNothing(),
      StartPresentationContext::PresentationConnectionErrorCallback error_cb =
          base::DoNothing()) {
    return std::make_unique<StartPresentationContext>(
        presentation_request, std::move(success_cb), std::move(error_cb));
  }

  // Simulate a supplementalNotification for |web_contents()|.
  std::string SimulateSupplementalNotification() {
    auto presentation_request = CreatePresentationRequest();

    // Create a SupplementalDevicePickerItem.
    service()->OnStartPresentationContextCreated(
        CreateStartPresentationContext(presentation_request));
    base::RunLoop().RunUntilIdle();
    auto notification_id =
        service()
            ->supplemental_device_picker_producer_
            ->GetOrCreateNotificationItem(base::UnguessableToken::Create())
            .id();
    EXPECT_FALSE(notification_id.empty());
    auto item = service()
                    ->presentation_request_notification_producer_
                    ->GetNotificationItem();
    EXPECT_TRUE(item);
    EXPECT_EQ(item->request(), presentation_request);
    return notification_id;
  }

  void SetMediaRoutesManagedByPresentationManager(
      std::vector<media_router::MediaRoute> routes) {
    ON_CALL(*presentation_manager_, GetMediaRoutes())
        .WillByDefault(Return(routes));
  }

  base::WeakPtr<PresentationRequestNotificationItem>
  GetSupplementalNotification() {
    return service()
        ->presentation_request_notification_producer_->GetNotificationItem();
  }

  template <class T>
  mojo::PendingRemote<T> TakeRemote(mojo::Receiver<T>& receiver,
                                    int expected_disconnect_count) {
    auto remote = receiver.BindNewPipeAndPassRemote();
    receiver.set_disconnect_handler(receiver_disconnect_handler_.Get());
    EXPECT_CALL(receiver_disconnect_handler_, Run())
        .Times(expected_disconnect_count);
    return remote;
  }

  template <class T>
  mojo::PendingRemote<T> TakeRemoteAndExpectDisconnect(
      mojo::Receiver<T>& receiver) {
    return TakeRemote(receiver, /*expected_disconnect_count=*/1);
  }

  template <class T>
  mojo::PendingRemote<T> TakeRemoteAndExpectNoDisconnect(
      mojo::Receiver<T>& receiver) {
    return TakeRemote(receiver, /*expected_disconnect_count=*/0);
  }

  template <class T>
  mojo::PendingReceiver<T> TakeReceiver(mojo::Remote<T>& remote,
                                        int expected_disconnect_count) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    remote.set_disconnect_handler(remote_disconnect_handler_.Get());
    EXPECT_CALL(remote_disconnect_handler_, Run())
        .Times(expected_disconnect_count);
    return receiver;
  }

  template <class T>
  mojo::PendingReceiver<T> TakeReceiverAndExpectDisconnect(
      mojo::Remote<T>& remote) {
    return TakeReceiver(remote, /*expected_disconnect_count=*/1);
  }

  template <class T>
  mojo::PendingReceiver<T> TakeReceiverAndExpectNoDisconnect(
      mojo::Remote<T>& remote) {
    return TakeReceiver(remote, /*expected_disconnect_count=*/0);
  }

 private:
  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;
  base::test::ScopedFeatureList feature_list_;
  base::MockCallback<base::OnceClosure> remote_disconnect_handler_;
  base::MockCallback<base::OnceClosure> receiver_disconnect_handler_;
};

// CastMediaNotificationProducer is owned by
// CastMediaNotificationProducerKeyedService in Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(MediaNotificationServiceCastTest,
       ShowCastSessionsForPresentationRequest) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate>
      dialog_delegate;

  std::unique_ptr<content::WebContents> web_contents_1(
      content::RenderViewHostTestHarness::CreateTestWebContents());
  std::unique_ptr<content::WebContents> web_contents_2(
      content::RenderViewHostTestHarness::CreateTestWebContents());

  // Simulate a Cast notification.
  const std::string id_1 = "route_id";
  auto media_route = CreateMediaRoute(id_1);
  SimulateMediaRoutesUpdate({media_route});

  // Simulate a Media session notification in |web_contents_2|.
  auto id_2 =
      SimulatePlayingControllableMediaForWebContents(web_contents_2.get());

  // Open the dialog from |web_contents_1|. Overwrite the return value of
  // GetMediaRoutes() so that MediaNotificationService associates
  // |web_contents_1| with the cast notification.
  SetMediaRoutesManagedByPresentationManager({media_route});
  EXPECT_CALL(dialog_delegate, ShowMediaItem(id_1, _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate,
                                             web_contents_1.get());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  // Open the dialog from |web_contents_2|, which has a media session
  // notification and no cast session notification.
  SetMediaRoutesManagedByPresentationManager({});
  EXPECT_CALL(dialog_delegate, ShowMediaItem(id_2.ToString(), _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate,
                                             web_contents_2.get());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();
}
#endif

TEST_F(MediaNotificationServiceCastTest, ShowMediaItemsForPresentationRequest) {
  std::unique_ptr<content::WebContents> web_contents_1(
      content::RenderViewHostTestHarness::CreateTestWebContents());
  std::unique_ptr<content::WebContents> web_contents_2(
      content::RenderViewHostTestHarness::CreateTestWebContents());

  // Simulate two active media sessions.
  auto id_1 =
      SimulatePlayingControllableMediaForWebContents(web_contents_1.get());
  auto id_2 =
      SimulatePlayingControllableMediaForWebContents(web_contents_2.get());

  // If the dialog is opened for a presentation request from |web_contents_1|,
  // only the media session with |id_1| should show up.
  NiceMock<global_media_controls::test::MockMediaDialogDelegate>
      dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaItem(id_1.ToString(), _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate,
                                             web_contents_1.get());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  // If the dialog is opened for a presentation request from |web_contents_2|,
  // only the media session with |id_2| should show up.
  EXPECT_CALL(dialog_delegate, ShowMediaItem(id_2.ToString(), _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate,
                                             web_contents_2.get());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();
}

// SupplementalDevicePickerProducer is not owned by MediaNotificationService
// on Chrome OS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(MediaNotificationServiceCastTest, ShowSupplementalNotifications) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate>
      dialog_delegate;
  // Do not show a supplemental notification if there is no start presentation
  // request context.
  EXPECT_FALSE(GetSupplementalNotification());
  EXPECT_CALL(dialog_delegate, ShowMediaItem(_, _)).Times(0);
  SimulateDialogOpened(&dialog_delegate);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  // Create a SupplementalDevicePickerItem.
  auto supplemental_notification_id = SimulateSupplementalNotification();

  // Open the dialog and a supplemental notification should show up.
  EXPECT_CALL(dialog_delegate, ShowMediaItem(supplemental_notification_id, _));
  SimulateDialogOpened(&dialog_delegate);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  // Closing the dialog has deleted the notification. Create another one.
  supplemental_notification_id = SimulateSupplementalNotification();
  EXPECT_CALL(dialog_delegate, ShowMediaItem(supplemental_notification_id, _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate, web_contents());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  // If there are notifications from other WebContents, still show dummy
  // notifications.
  std::unique_ptr<content::WebContents> test_web_contents(
      content::RenderViewHostTestHarness::CreateTestWebContents());
  auto media_session_id =
      SimulatePlayingControllableMediaForWebContents(test_web_contents.get());
  supplemental_notification_id = SimulateSupplementalNotification();
  // Create a cast session not associated with any WebContents.
  const std::string route_id = "route_id";
  SimulateMediaRoutesUpdate({CreateMediaRoute(route_id)});
  EXPECT_CALL(dialog_delegate, ShowMediaItem(route_id, _));
  EXPECT_CALL(dialog_delegate, ShowMediaItem(media_session_id.ToString(), _));
  EXPECT_CALL(dialog_delegate, ShowMediaItem(supplemental_notification_id, _));
  SimulateDialogOpened(&dialog_delegate);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();
}

TEST_F(MediaNotificationServiceCastTest, HideSupplementalNotifications) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate>
      dialog_delegate;
  auto supplemental_notification_id = SimulateSupplementalNotification();
  // If there is a media session, hide the supplemental notification.
  auto media_session_id =
      SimulatePlayingControllableMediaForWebContents(web_contents());

  EXPECT_CALL(dialog_delegate, ShowMediaItem(media_session_id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  EXPECT_CALL(dialog_delegate, ShowMediaItem(media_session_id.ToString(), _));
  SimulateDialogOpenedForPresentationRequest(&dialog_delegate, web_contents());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  SimulateCloseDialog();

  SimulateFocusLost(media_session_id);
  // If there is a cast session, hide the supplemental notification.
  auto media_route = CreateMediaRoute("route_id");
  SimulateMediaRoutesUpdate({media_route});

  SetMediaRoutesManagedByPresentationManager({media_route});
  service()->media_item_manager()->OnItemsChanged();
  EXPECT_CALL(dialog_delegate, ShowMediaItem(media_route.media_route_id(), _));
  SimulateDialogOpened(&dialog_delegate);
  SimulateCloseDialog();

  EXPECT_CALL(dialog_delegate, ShowMediaItem(media_route.media_route_id(), _));
  SimulateDialogOpened(&dialog_delegate);
  SimulateCloseDialog();
}

TEST_F(MediaNotificationServiceCastTest,
       OnStartPresentationContextCreated_ForPresentationRequestNotifications) {
  // If there does not exist an active notification, pass the
  // StartPresentationContext to SupplementalDevicePickerProducer.
  service()->OnStartPresentationContextCreated(
      CreateStartPresentationContext(CreatePresentationRequest()));
  auto supplemental_notification = GetSupplementalNotification();
  EXPECT_TRUE(supplemental_notification);
  EXPECT_FALSE(supplemental_notification->is_default_presentation_request());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(MediaNotificationServiceCastTest,
       OnStartPresentationContextCreated_ForMediaSessionNotifications) {
  SimulatePlayingControllableMediaForWebContents(web_contents());
  base::MockCallback<content::PresentationConnectionErrorCallback>
      mock_error_cb;
  auto context = CreateStartPresentationContext(
      CreatePresentationRequest(), base::DoNothing(), mock_error_cb.Get());
  auto* context_ptr = context.get();

  // If there only exists a media session notification, pass |context| to
  // global_media_controls::MediaSessionItemProducer.
  EXPECT_CALL(mock_error_cb, Run).Times(0);
  service()->OnStartPresentationContextCreated(std::move(context));

  // Invoke callback before |mock_error_cb| is deleted.
  testing::Mock::VerifyAndClearExpectations(&mock_error_cb);
  context_ptr->InvokeErrorCallback(blink::mojom::PresentationError(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED, ""));
}

TEST_F(MediaNotificationServiceCastTest,
       OnStartPresentationContextCreated_ForCastNotifications) {
  auto media_route = CreateMediaRoute("route_id");
  SetMediaRoutesManagedByPresentationManager({media_route});

  // If there exists cast notifications, |context| will not be passed to any
  // notification producer and its error callback should be evoked.
  base::MockCallback<content::PresentationConnectionErrorCallback>
      mock_error_cb;
  EXPECT_CALL(mock_error_cb, Run);
  service()->OnStartPresentationContextCreated(CreateStartPresentationContext(
      CreatePresentationRequest(), base::DoNothing(), mock_error_cb.Get()));
}

TEST_F(MediaNotificationServiceCastTest,
       OnStartPresentationContextCreated_ForRemovedWebContents) {
  // If the StartPresentationContext is from a WebContents that has been
  // removed, its error callback should be evoked.
  base::MockCallback<content::PresentationConnectionErrorCallback>
      mock_error_cb;
  auto context = CreateStartPresentationContext(
      CreatePresentationRequest(), base::DoNothing(), mock_error_cb.Get());
  EXPECT_CALL(mock_error_cb, Run);
  DeleteContents();
  service()->OnStartPresentationContextCreated(std::move(context));
}

TEST_F(MediaNotificationServiceCastTest, GetDeviceListHostForSession) {
  mojo::Remote<mojom::DeviceListHost> host_remote;
  MockDeviceListClient client;
  auto id = SimulatePlayingControllableMediaForWebContents(web_contents());
  service()->GetDeviceListHostForSession(
      id.ToString(), TakeReceiverAndExpectNoDisconnect(host_remote),
      TakeRemoteAndExpectNoDisconnect(client.receiver()));
  host_remote.FlushForTesting();
  client.receiver().FlushForTesting();
}

TEST_F(MediaNotificationServiceCastTest,
       DisconnectOnGetDeviceListHostForInvalidSession) {
  mojo::Remote<mojom::DeviceListHost> host_remote;
  MockDeviceListClient client;
  service()->GetDeviceListHostForSession(
      "invalid_id", TakeReceiverAndExpectDisconnect(host_remote),
      TakeRemoteAndExpectDisconnect(client.receiver()));
  host_remote.FlushForTesting();
  client.receiver().FlushForTesting();
}

TEST_F(MediaNotificationServiceCastTest, GetDeviceListHostForPresentation) {
  mojo::Remote<mojom::DeviceListHost> host_remote;
  MockDeviceListClient client;
  service()->OnStartPresentationContextCreated(
      CreateStartPresentationContext(CreatePresentationRequest()));
  service()->GetDeviceListHostForPresentation(
      TakeReceiverAndExpectNoDisconnect(host_remote),
      TakeRemoteAndExpectNoDisconnect(client.receiver()));
  host_remote.FlushForTesting();
  client.receiver().FlushForTesting();
}

TEST_F(MediaNotificationServiceCastTest,
       DisconnectOnGetDeviceListHostForNonexistentPresentation) {
  mojo::Remote<mojom::DeviceListHost> host_remote;
  MockDeviceListClient client;
  service()->GetDeviceListHostForPresentation(
      TakeReceiverAndExpectDisconnect(host_remote),
      TakeRemoteAndExpectDisconnect(client.receiver()));
  host_remote.FlushForTesting();
  client.receiver().FlushForTesting();
}

TEST_F(MediaNotificationServiceCastTest, DeleteDeviceListHostOnShutdown) {
  mojo::Remote<mojom::DeviceListHost> host_remote;
  MockDeviceListClient client;
  auto id = SimulatePlayingControllableMediaForWebContents(web_contents());
  service()->GetDeviceListHostForSession(
      id.ToString(), TakeReceiverAndExpectDisconnect(host_remote),
      TakeRemoteAndExpectDisconnect(client.receiver()));

  // Shutdown() should cause a disconnect, fulfilling the expectations in
  // TakeReceiver...() and TakeRemote...() above.
  service()->Shutdown();
  host_remote.FlushForTesting();
  client.receiver().FlushForTesting();
}

TEST_F(MediaNotificationServiceCastTest,
       CreateCastDialogControllerWithRemotePlayback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);

  auto id = SimulatePlayingControllableMediaForWebContents(web_contents());
  auto* session_item = GetNotificationSessionItem(id);
  ASSERT_NE(nullptr, session_item);

  // At this point `session_item` has no RemotePlaybackMetadata and there's no
  // default MediaSource.
  std::unique_ptr<media_router::CastDialogController> controller_presentation =
      service()->CreateCastDialogControllerForSession(id.ToString());
  std::unique_ptr<media_router::MediaRouteStarter> starter =
      controller_presentation->TakeMediaRouteStarter();
  const auto* query_result_manager = starter->GetQueryResultManagerForTesting();
  EXPECT_TRUE(query_result_manager->GetSupportedCastModes().empty());

  // Send MediaSessionInfo with RemotePlaybackMetadata to `session_item`.
  auto session_info = MediaSessionInfo::New();
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New("vp8", "opus", false,
                                                        false, "", false);
  session_info->is_controllable = true;
  session_item->MediaSessionInfoChanged(std::move(session_info));

  std::unique_ptr<media_router::CastDialogController>
      controller_remote_playback =
          service()->CreateCastDialogControllerForSession(id.ToString());
  starter = controller_remote_playback->TakeMediaRouteStarter();
  query_result_manager = starter->GetQueryResultManagerForTesting();
  const media_router::CastModeSet mode = {
      media_router::MediaCastMode::REMOTE_PLAYBACK};
  EXPECT_EQ(mode, query_result_manager->GetSupportedCastModes());
}

TEST_F(MediaNotificationServiceCastTest, RequestMediaRemoting) {
  service()->OnMediaRemotingRequested("invalid_item_id");
  auto id = base::UnguessableToken::Create();
  SimulatePlayingControllableMedia(id);
  // TODO(takumif): Confirm that this calls the MediaNotificationItem.
  service()->OnMediaRemotingRequested(id.ToString());
}

TEST_F(MediaNotificationServiceCastTest, OnSinksDiscoveredForLocalMedia) {
  // Playing the media.
  auto id = SimulatePlayingControllableMediaForWebContents(web_contents());

  NiceMock<global_media_controls::test::MockMediaDialogDelegate>
      dialog_delegate;

  // Opening the dialog.
  SimulateDialogOpened(&dialog_delegate);

  service()->OnSinksDiscovered(id.ToString());
  EXPECT_FALSE(service()->should_show_cast_local_media_iph());

  // Navigating to a page with local media.
  NavigateAndCommit(GURL("file:///example.mp4"));

  service()->OnSinksDiscovered(id.ToString());
  EXPECT_TRUE(service()->should_show_cast_local_media_iph());
}
