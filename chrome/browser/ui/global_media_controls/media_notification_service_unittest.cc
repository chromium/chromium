// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;
using testing::AtLeast;
using testing::Expectation;
using testing::Return;

namespace {

class MockMediaNotificationServiceObserver
    : public MediaNotificationServiceObserver {
 public:
  MockMediaNotificationServiceObserver() = default;
  MockMediaNotificationServiceObserver(
      const MockMediaNotificationServiceObserver&) = delete;
  MockMediaNotificationServiceObserver& operator=(
      const MockMediaNotificationServiceObserver&) = delete;
  ~MockMediaNotificationServiceObserver() override = default;

  // MediaNotificationServiceObserver implementation.
  MOCK_METHOD(void, OnNotificationListChanged, ());
  MOCK_METHOD(void, OnMediaDialogOpened, ());
  MOCK_METHOD(void, OnMediaDialogClosed, ());
};

class MockMediaDialogDelegate : public MediaDialogDelegate {
 public:
  MockMediaDialogDelegate() = default;
  ~MockMediaDialogDelegate() override { Close(); }

  void Open(MediaNotificationService* service) {
    ASSERT_NE(nullptr, service);
    service_ = service;
    service_->SetDialogDelegate(this);
  }

  void Close() {
    if (!service_)
      return;

    service_->SetDialogDelegate(nullptr);
    service_ = nullptr;
  }

  // MediaDialogDelegate implementation.
  MOCK_METHOD2(
      ShowMediaSession,
      MediaNotificationContainerImpl*(
          const std::string& id,
          base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD1(HideMediaSession, void(const std::string& id));
  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds) {
    return std::unique_ptr<OverlayMediaNotification>(PopOutProxy(id, bounds));
  }

  // Need to use a proxy since std::unique_ptr is not copyable.
  MOCK_METHOD2(PopOutProxy,
               OverlayMediaNotification*(const std::string& id,
                                         gfx::Rect bounds));

 private:
  MediaNotificationService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaDialogDelegate);
};

class MockOverlayMediaNotification : public OverlayMediaNotification {
 public:
  MockOverlayMediaNotification() = default;
  MockOverlayMediaNotification(const MockOverlayMediaNotification&) = delete;
  MockOverlayMediaNotification& operator=(const MockOverlayMediaNotification&) =
      delete;
  ~MockOverlayMediaNotification() override = default;

  OverlayMediaNotificationsManager* manager() { return manager_; }

  // MockOverlayMediaNotification implementation.
  void SetManager(OverlayMediaNotificationsManager* manager) {
    manager_ = manager;
    SetManagerProxy(manager);
  }
  MOCK_METHOD0(ShowNotification, void());
  MOCK_METHOD0(CloseNotification, void());

  // Use a proxy so we can add expectations on it while also storing the
  // manager for future use.
  MOCK_METHOD1(SetManagerProxy,
               void(OverlayMediaNotificationsManager* manager));

 private:
  OverlayMediaNotificationsManager* manager_ = nullptr;
};

}  // anonymous namespace

class MediaNotificationServiceTest : public testing::Test {
 public:
  MediaNotificationServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI) {}
  ~MediaNotificationServiceTest() override = default;

  void SetUp() override {
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&media_router::MockMediaRouter::Create));
    service_ = std::make_unique<MediaNotificationService>(&profile_, false);
    service_->AddObserver(&observer_);
  }

  void TearDown() override { service_.reset(); }

 protected:
  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(
        base::TimeDelta::FromMilliseconds(milliseconds));
  }

  void AdvanceClockMinutes(int minutes) {
    AdvanceClockMilliseconds(1000 * 60 * minutes);
  }

  base::UnguessableToken SimulatePlayingControllableMedia() {
    base::UnguessableToken id = base::UnguessableToken::Create();
    SimulateFocusGained(id, true);
    SimulateNecessaryMetadata(id);
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
    service_->OnFocusGained(CreateFocusRequest(id, controllable));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    service_->OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the
    // MediaNotificationService to show a media session, that session needs
    // a title and artist. Typically this would happen through the media session
    // service, but since the service doesn't run for this test, we'll manually
    // grab the MediaNotificationItem from the MediaNotificationService and
    // set the metadata.
    auto item_itr = service_->sessions_.find(id.ToString());
    ASSERT_NE(service_->sessions_.end(), item_itr);

    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_itr->second.item()->MediaSessionMetadataChanged(std::move(metadata));
  }

  void SimulateHasArtwork(const base::UnguessableToken& id) {
    auto item_itr = service_->sessions_.find(id.ToString());
    ASSERT_NE(service_->sessions_.end(), item_itr);

    SkBitmap image;
    image.allocN32Pixels(10, 10);
    image.eraseColor(SK_ColorMAGENTA);

    item_itr->second.item()->MediaControllerImageChanged(
        media_session::mojom::MediaSessionImageType::kArtwork, image);
  }

  void SimulateHasNoArtwork(const base::UnguessableToken& id) {
    auto item_itr = service_->sessions_.find(id.ToString());
    ASSERT_NE(service_->sessions_.end(), item_itr);

    item_itr->second.item()->MediaControllerImageChanged(
        media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());
  }

  void SimulateReceivedAudioFocusRequests(
      std::vector<AudioFocusRequestStatePtr> requests) {
    service_->OnReceivedAudioFocusRequests(std::move(requests));
  }

  bool IsSessionFrozen(const base::UnguessableToken& id) const {
    auto item_itr = service_->sessions_.find(id.ToString());
    EXPECT_NE(service_->sessions_.end(), item_itr);
    return item_itr->second.item()->frozen();
  }

  bool IsSessionInactive(const base::UnguessableToken& id) const {
    return base::Contains(service_->inactive_session_ids_, id.ToString());
  }

  bool HasActiveNotifications() const {
    return service_->HasActiveNotifications();
  }

  bool HasFrozenNotifications() const {
    return service_->HasFrozenNotifications();
  }

  bool HasOpenDialog() const { return service_->HasOpenDialog(); }

  void SimulateDialogOpened(MockMediaDialogDelegate* delegate) {
    delegate->Open(service_.get());
  }

  void SimulateTabClosed(const base::UnguessableToken& id) {
    // When a tab is closing, audio focus will be lost before the WebContents is
    // destroyed, so to simulate closer to reality we will also simulate audio
    // focus lost here.
    SimulateFocusLost(id);

    // Now, close the tab. The session may have been destroyed with
    // |SimulateFocusLost()| above.
    auto item_itr = service_->sessions_.find(id.ToString());
    if (item_itr != service_->sessions_.end())
      item_itr->second.WebContentsDestroyed();
  }

  void SimulatePlaybackStateChanged(const base::UnguessableToken& id,
                                    bool playing) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;
    session_info->playback_state =
        playing ? media_session::mojom::MediaPlaybackState::kPlaying
                : media_session::mojom::MediaPlaybackState::kPaused;

    auto item_itr = service_->sessions_.find(id.ToString());
    EXPECT_NE(service_->sessions_.end(), item_itr);
    item_itr->second.MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateMediaSeeked(const base::UnguessableToken& id) {
    auto item_itr = service_->sessions_.find(id.ToString());
    EXPECT_NE(service_->sessions_.end(), item_itr);
    item_itr->second.MediaSessionPositionChanged(base::nullopt);
  }

  void SimulateNotificationClicked(const base::UnguessableToken& id) {
    service_->OnContainerClicked(id.ToString());
  }

  void SimulateDismissButtonClicked(const base::UnguessableToken& id) {
    service_->OnContainerDismissed(id.ToString());
  }

  // Simulates the media notification of the given |id| being dragged out of the
  // given dialog.
  MockOverlayMediaNotification* SimulateNotificationDraggedOut(
      const base::UnguessableToken& id,
      MockMediaDialogDelegate* dialog_delegate) {
    const gfx::Rect dragged_out_bounds(0, 1, 2, 3);
    auto overlay_notification_unique =
        std::make_unique<MockOverlayMediaNotification>();
    MockOverlayMediaNotification* overlay_notification =
        overlay_notification_unique.get();

    // When the notification is dragged out, the dialog should be asked to
    // remove the notification and return an overlay version of it.
    EXPECT_CALL(*dialog_delegate,
                PopOutProxy(id.ToString(), dragged_out_bounds))
        .WillOnce(Return(overlay_notification_unique.release()));

    // Then, that overlay notification should receive a manager and be shown.
    Expectation set_manager =
        EXPECT_CALL(*overlay_notification, SetManagerProxy(_));
    EXPECT_CALL(*overlay_notification, ShowNotification()).After(set_manager);

    // Fire the drag out.
    service_->OnContainerDraggedOut(id.ToString(), dragged_out_bounds);
    testing::Mock::VerifyAndClearExpectations(dialog_delegate);
    testing::Mock::VerifyAndClearExpectations(overlay_notification);

    return overlay_notification;
  }

  void ExpectHistogramCountRecorded(int count, int size) {
    histogram_tester_.ExpectBucketCount(
        media_message_center::kCountHistogramName, count, size);
  }

  void ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        "Media.GlobalMediaControls.DismissReason", reason, count);
  }

  void ExpectHistogramInteractionDelayAfterPause(base::TimeDelta time,
                                                 int count) {
    histogram_tester_.ExpectTimeBucketCount(
        "Media.GlobalMediaControls.InteractionDelayAfterPause", time, count);
  }

  void ExpectEmptyInteractionHistogram() {
    histogram_tester_.ExpectTotalCount(
        "Media.GlobalMediaControls.InteractionDelayAfterPause", 0);
  }

  void SimulateMediaRoutesUpdate(
      const std::vector<media_router::MediaRoute>& routes) {
    service_->cast_notification_provider_->OnRoutesUpdated(routes, {});
  }

  MockMediaNotificationServiceObserver& observer() { return observer_; }

  MediaNotificationService* service() { return service_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockMediaNotificationServiceObserver observer_;
  std::unique_ptr<MediaNotificationService> service_;
  base::HistogramTester histogram_tester_;
  TestingProfile profile_;
};

// TODO(takumif): Remove this class once |kGlobalMediaControlsForCast| is
// enabled by default.
class MediaNotificationServiceCastTest : public MediaNotificationServiceTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsForCast);
    MediaNotificationServiceTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaNotificationServiceTest, ShowControllableOnGainAndHideOnLoss) {
  // Simulate a new active, controllable media session.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  EXPECT_FALSE(HasActiveNotifications());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_FALSE(IsSessionFrozen(id));
  EXPECT_TRUE(HasActiveNotifications());

  // Ensure that the observer was notified of the new notification.
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Simulate opening a MediaDialogView.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  EXPECT_CALL(observer(), OnMediaDialogOpened());
  EXPECT_FALSE(HasOpenDialog());
  SimulateDialogOpened(&dialog_delegate);

  // Ensure that the session was shown.
  ExpectHistogramCountRecorded(1, 1);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Ensure that the observer was notified of the dialog opening.
  EXPECT_TRUE(HasOpenDialog());
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Simulate the active session ending.
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString())).Times(0);
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  EXPECT_FALSE(HasFrozenNotifications());
  SimulateFocusLost(id);

  // Ensure that the session was frozen and not hidden.
  EXPECT_TRUE(IsSessionFrozen(id));
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Ensure that the observer was notification of the frozen notification.
  EXPECT_TRUE(HasFrozenNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());

  service()->ShowNotification(id.ToString());

  // Once the freeze timer fires, we should hide the media session.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString()));
  AdvanceClockMilliseconds(2500);
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationServiceTest, DoesNotShowUncontrollableSession) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  // When focus is gained, we should not show an active session.
  EXPECT_FALSE(HasActiveNotifications());
  SimulateFocusGained(id, false);
  SimulateNecessaryMetadata(id);
  EXPECT_FALSE(HasActiveNotifications());

  // When focus is lost, we should not have a frozen session.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasFrozenNotifications());

  // When focus is regained, we should still not have an active session.
  SimulateFocusGained(id, false);
  EXPECT_FALSE(HasActiveNotifications());
}

TEST_F(MediaNotificationServiceTest,
       DoesNotShowControllableSessionThatBecomesUncontrollable) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Lose focus so the item freezes.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // After 1s, the item should still be frozen.
  AdvanceClockMilliseconds(1000);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // If the item regains focus but is not controllable, it should not become
  // active.
  SimulateFocusGained(id, false);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // And the frozen timer should still fire after the initial 2.5 seconds is
  // finished.
  AdvanceClockMilliseconds(1400);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  AdvanceClockMilliseconds(200);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_FALSE(HasFrozenNotifications());
}

TEST_F(MediaNotificationServiceTest, ShowsAllInitialControllableSessions) {
  base::UnguessableToken controllable1_id = base::UnguessableToken::Create();
  base::UnguessableToken uncontrollable_id = base::UnguessableToken::Create();
  base::UnguessableToken controllable2_id = base::UnguessableToken::Create();

  std::vector<AudioFocusRequestStatePtr> requests;
  requests.push_back(CreateFocusRequest(controllable1_id, true));
  requests.push_back(CreateFocusRequest(uncontrollable_id, false));
  requests.push_back(CreateFocusRequest(controllable2_id, true));

  EXPECT_FALSE(HasActiveNotifications());

  // Having controllable sessions should count as active.
  SimulateReceivedAudioFocusRequests(std::move(requests));

  SimulateNecessaryMetadata(controllable1_id);
  SimulateNecessaryMetadata(uncontrollable_id);
  SimulateNecessaryMetadata(controllable2_id);

  EXPECT_TRUE(HasActiveNotifications());

  // If we open a dialog, it should be told to show the controllable sessions,
  // but not the uncontrollable one.
  MockMediaDialogDelegate dialog_delegate;

  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(controllable1_id.ToString(), _));
  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(uncontrollable_id.ToString(), _))
      .Times(0);
  EXPECT_CALL(dialog_delegate,
              ShowMediaSession(controllable2_id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Ensure that we properly recorded the number of active sessions shown.
  ExpectHistogramCountRecorded(2, 1);
}

TEST_F(MediaNotificationServiceTest, HideAfterTimeoutAndActiveAgainOnPlay) {
  // First, start an active session.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, stop playing media so the session is frozen, but not yet hidden.
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kMediaSessionStopped,
      0);
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kMediaSessionStopped,
      0);

  // If the time hasn't elapsed yet, the session should still be frozen.
  AdvanceClockMilliseconds(2400);
  EXPECT_TRUE(HasFrozenNotifications());
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kMediaSessionStopped,
      0);

  // Once the time is elapsed, the session should be hidden.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  AdvanceClockMilliseconds(200);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_FALSE(HasFrozenNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kMediaSessionStopped,
      1);

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationServiceTest,
       BecomesActiveIfMediaStartsPlayingWithinTimeout) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, stop playing media so the session is frozen, but hasn't been hidden
  // yet.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // If the time hasn't elapsed yet, we should still not be hidden.
  AdvanceClockMilliseconds(2400);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // If media starts playing again, we should become active again.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationServiceTest, NewMediaSessionWhileDialogOpen) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);
  ExpectHistogramCountRecorded(1, 1);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Then, have a new media session start while the dialog is opened. This
  // should update the dialog.
  base::UnguessableToken new_id = base::UnguessableToken::Create();
  EXPECT_CALL(dialog_delegate, ShowMediaSession(new_id.ToString(), _));
  SimulateFocusGained(new_id, true);
  SimulateNecessaryMetadata(new_id);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // If we close this dialog and open a new one, the new one should receive both
  // media sessions immediately.
  dialog_delegate.Close();
  MockMediaDialogDelegate new_dialog;
  EXPECT_CALL(new_dialog, ShowMediaSession(id.ToString(), _));
  EXPECT_CALL(new_dialog, ShowMediaSession(new_id.ToString(), _));
  SimulateDialogOpened(&new_dialog);
  ExpectHistogramCountRecorded(1, 1);
  ExpectHistogramCountRecorded(2, 1);
}

TEST_F(MediaNotificationServiceTest,
       SessionIsRemovedImmediatelyWhenATabCloses) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, close the tab. The session should immediately be hidden.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::kTabClosed,
      0);
  SimulateTabClosed(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_FALSE(HasFrozenNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::kTabClosed,
      1);
}

TEST_F(MediaNotificationServiceTest, DismissesMediaSession) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, click the dismiss button. This should stop and hide the session.
  EXPECT_CALL(dialog_delegate, HideMediaSession(id.ToString()));
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kUserDismissedNotification,
      0);
  SimulateDismissButtonClicked(id);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kUserDismissedNotification,
      1);
}

// TODO(https://crbug.com/1034406) Flaky on Mac10.12, Linux and Win10.
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
#define MAYBE_CountCastSessionsAsActive DISABLED_CountCastSessionsAsActive
#else
#define MAYBE_CountCastSessionsAsActive CountCastSessionsAsActive
#endif
TEST_F(MediaNotificationServiceCastTest, MAYBE_CountCastSessionsAsActive) {
  media_router::MediaRoute media_route("id",
                                       media_router::MediaSource("source_id"),
                                       "sink_id", "description", true, true);
  media_route.set_controller_type(media_router::RouteControllerType::kGeneric);

  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  EXPECT_FALSE(HasActiveNotifications());
  SimulateMediaRoutesUpdate({media_route});
  EXPECT_TRUE(HasActiveNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  SimulateMediaRoutesUpdate({});
  EXPECT_FALSE(HasActiveNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
}

// Regression test for https://crbug.com/1015903: we could end up in a situation
// where the toolbar icon was disabled indefinitely.
TEST_F(MediaNotificationServiceTest, LoseGainLoseDoesNotCauseRaceCondition) {
  // First, start an active session and include artwork.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  SimulateHasArtwork(id);
  EXPECT_TRUE(HasActiveNotifications());

  // Then, stop playing media so the session is frozen, but hasn't been hidden
  // yet.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // Simulate no artwork, so we wait for new artwork.
  SimulateHasNoArtwork(id);

  // Simulate regaining focus, but no artwork yet so we wait.
  SimulateFocusGained(id, true);
  SimulateNecessaryMetadata(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // Then, lose focus again before getting artwork.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(HasFrozenNotifications());

  // When the freeze timer fires, we should be hidden.
  EXPECT_CALL(observer(), OnNotificationListChanged()).Times(AtLeast(1));
  AdvanceClockMilliseconds(2600);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_FALSE(HasFrozenNotifications());
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationServiceTest, ShowsOverlayForDraggedOutNotifications) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Drag out the notification.
  MockOverlayMediaNotification* overlay_notification =
      SimulateNotificationDraggedOut(id, &dialog_delegate);

  // Now, dismiss the notification. Since the notification is an overlay
  // notification, this should just close the overlay notification.
  EXPECT_CALL(*overlay_notification, CloseNotification());
  SimulateDismissButtonClicked(id);
  testing::Mock::VerifyAndClearExpectations(overlay_notification);

  // After we close, we notify our manager, and the dialog should be informed
  // that it can show the notification again.
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  overlay_notification->manager()->OnOverlayNotificationClosed(id.ToString());
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);
}

TEST_F(MediaNotificationServiceTest, HidesInactiveNotifications) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveNotifications());

  // But once it's been inactive for over an hour, it should disappear.
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kInactiveTimeout,
      0);
  AdvanceClockMinutes(2);
  EXPECT_FALSE(HasActiveNotifications());
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kInactiveTimeout,
      1);

  // Since the user never interacted with the media before it was paused, we
  // should not have recorded any post-pause interactions.
  ExpectEmptyInteractionHistogram();

  // If we now close the tab, then it shouldn't record that as the dismiss
  // reason, since we already recorded a reason.
  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::kTabClosed,
      0);

  SimulateTabClosed(id);

  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::kTabClosed,
      0);
}

TEST_F(MediaNotificationServiceTest, InactiveBecomesActive_PlayPause) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());
  EXPECT_FALSE(IsSessionInactive(id));

  // Let the notification become inactive.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(IsSessionInactive(id));

  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(70),
                                            0);

  // Then, play the media. The notification should become active.
  SimulatePlaybackStateChanged(id, true);

  // We should have recorded an interaction even though the timer has finished.
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(70),
                                            1);
  EXPECT_TRUE(HasActiveNotifications());
  EXPECT_FALSE(IsSessionInactive(id));
}

TEST_F(MediaNotificationServiceTest, InactiveBecomesActive_Seeking) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());
  EXPECT_FALSE(IsSessionInactive(id));

  // Let the notification become inactive.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(IsSessionInactive(id));

  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(70),
                                            0);

  // Then, seek the media. The notification should become active.
  SimulateMediaSeeked(id);

  // We should have recorded an interaction even though the timer has finished.
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(70),
                                            1);
  EXPECT_TRUE(HasActiveNotifications());
  EXPECT_FALSE(IsSessionInactive(id));

  // If we don't interact again, the notification should become inactive again.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveNotifications());
  EXPECT_TRUE(IsSessionInactive(id));
}

TEST_F(MediaNotificationServiceTest, DelaysHidingNotifications_PlayPause) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveNotifications());

  // If we start playing again, we should not hide the notification, even after
  // an hour.
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(59),
                                            0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(59),
                                            1);
  AdvanceClockMinutes(2);
  EXPECT_TRUE(HasActiveNotifications());

  // If we pause again, it should hide after an hour.
  SimulatePlaybackStateChanged(id, false);
  AdvanceClockMinutes(61);
  EXPECT_FALSE(HasActiveNotifications());
}

TEST_F(MediaNotificationServiceTest, DelaysHidingNotifications_Interactions) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveNotifications());

  // If the user clicks to go back to the tab, it should reset the hide timer.
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(59),
                                            0);
  SimulateNotificationClicked(id);
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(59),
                                            1);
  AdvanceClockMinutes(50);
  EXPECT_TRUE(HasActiveNotifications());

  // If the user seeks the media before an hour is up, it should reset the hide
  // timer.
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(50),
                                            0);
  SimulateMediaSeeked(id);
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(50),
                                            1);
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveNotifications());

  // After the hour has passed, the notification should hide.
  AdvanceClockMinutes(2);
  EXPECT_FALSE(HasActiveNotifications());
}

TEST_F(MediaNotificationServiceTest,
       DelaysHidingNotifications_OverlayThenPause) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, pull out the notification into an overlay notification.
  MockOverlayMediaNotification* overlay_notification =
      SimulateNotificationDraggedOut(id, &dialog_delegate);

  // Then, pause the media.
  SimulatePlaybackStateChanged(id, false);

  // Since the notification is in an overlay, it should never time out as
  // inactive.
  AdvanceClockMinutes(61);
  EXPECT_FALSE(IsSessionInactive(id));

  // Now, close the overlay notification.
  overlay_notification->manager()->OnOverlayNotificationClosed(id.ToString());

  // The notification should become inactive now that it's not in an overlay.
  AdvanceClockMinutes(61);
  EXPECT_TRUE(IsSessionInactive(id));
}

TEST_F(MediaNotificationServiceTest,
       DelaysHidingNotifications_PauseThenOverlay) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media.
  SimulatePlaybackStateChanged(id, false);

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, pull out the notification into an overlay notification.
  MockOverlayMediaNotification* overlay_notification =
      SimulateNotificationDraggedOut(id, &dialog_delegate);

  // Since the notification is in an overlay, it should never time out as
  // inactive.
  AdvanceClockMinutes(61);
  EXPECT_FALSE(IsSessionInactive(id));

  // Now, close the overlay notification.
  overlay_notification->manager()->OnOverlayNotificationClosed(id.ToString());

  // The notification should become inactive now that it's not in an overlay.
  AdvanceClockMinutes(61);
  EXPECT_TRUE(IsSessionInactive(id));
}

TEST_F(MediaNotificationServiceTest, HidingNotification_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kGlobalMediaControlsAutoDismiss);

  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());

  // After 61 minutes, the notification should still be there.
  AdvanceClockMinutes(61);
  EXPECT_TRUE(HasActiveNotifications());

  ExpectHistogramDismissReasonRecorded(
      MediaNotificationService::GlobalMediaControlsDismissReason::
          kInactiveTimeout,
      0);

  // Since the user never interacted with the media before it was paused, we
  // should not have recorded any post-pause interactions.
  ExpectEmptyInteractionHistogram();

  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(61),
                                            0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(base::TimeDelta::FromMinutes(61),
                                            1);
}

TEST_F(MediaNotificationServiceTest, HidingNotification_TimerParams) {
  const int kTimerInMinutes = 6;
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["timer_in_minutes"] = base::NumberToString(kTimerInMinutes);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kGlobalMediaControlsAutoDismiss, params);

  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveNotifications());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveNotifications());

  // After (kTimerInMinutes-1) minutes, the notification should still be there.
  AdvanceClockMinutes(kTimerInMinutes - 1);
  EXPECT_TRUE(HasActiveNotifications());

  // If we start playing again, we should not hide the notification, even after
  // kTimerInMinutes.
  ExpectHistogramInteractionDelayAfterPause(
      base::TimeDelta::FromMinutes(kTimerInMinutes - 1), 0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(
      base::TimeDelta::FromMinutes(kTimerInMinutes - 1), 1);
  AdvanceClockMinutes(2);
  EXPECT_TRUE(HasActiveNotifications());

  // If we pause again, it should hide after kTimerInMinutes.
  SimulatePlaybackStateChanged(id, false);
  AdvanceClockMinutes(kTimerInMinutes + 1);
  EXPECT_FALSE(HasActiveNotifications());
}
