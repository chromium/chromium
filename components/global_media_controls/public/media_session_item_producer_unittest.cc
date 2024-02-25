// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_session_item_producer.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/mock_audio_focus_manager.h"
#include "services/media_session/public/cpp/test/mock_media_controller_manager.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;
using testing::AtLeast;
using testing::Expectation;
using testing::NiceMock;
using testing::Return;

namespace global_media_controls {

class MediaSessionItemProducerTest : public testing::Test {
 public:
  MediaSessionItemProducerTest() = default;
  ~MediaSessionItemProducerTest() override = default;

  void SetUp() override {
    audio_focus_manager_ = std::make_unique<
        NiceMock<media_session::test::MockAudioFocusManager>>();
    media_controller_manager_ = std::make_unique<
        NiceMock<media_session::test::MockMediaControllerManager>>();

    testing::Mock::AllowLeak(audio_focus_manager_.get());

    EXPECT_CALL(*audio_focus_manager_, GetFocusRequests(_))
        .WillOnce(
            testing::Invoke([](media_session::test::MockAudioFocusManager::
                                   GetFocusRequestsCallback callback) {
              std::move(callback).Run(
                  std::vector<
                      media_session::mojom::AudioFocusRequestStatePtr>());
            }));

    mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote(
        audio_focus_manager_->GetPendingRemote());
    mojo::Remote<media_session::mojom::MediaControllerManager>
        controller_manager_remote(
            media_controller_manager_->GetPendingRemote());

    producer_ = std::make_unique<MediaSessionItemProducer>(
        std::move(audio_focus_remote), std::move(controller_manager_remote),
        &item_manager_, std::nullopt);

    audio_focus_manager_->Flush();
    testing::Mock::VerifyAndClearExpectations(audio_focus_manager_.get());
  }

  void TearDown() override {
    producer_.reset();
    media_controller_manager_.reset();
    audio_focus_manager_.reset();
  }

 protected:
  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(base::Milliseconds(milliseconds));
  }

  void AdvanceClockMinutes(int minutes) {
    AdvanceClockMilliseconds(1000 * 60 * minutes);
  }

  base::UnguessableToken SimulatePlayingControllableMedia() {
    return SimulatePlayingControllableMedia(base::UnguessableToken::Create());
  }

  base::UnguessableToken SimulatePlayingControllableMedia(
      base::UnguessableToken id) {
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
    producer_->OnFocusGained(CreateFocusRequest(id, controllable));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    producer_->OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the
    // MediaNotificationService to show a media session, that session needs
    // a title and artist. Typically this would happen through the media session
    // service, but since the service doesn't run for this test, we'll manually
    // grab the MediaNotificationItem from the MediaNotificationService and
    // set the metadata.
    auto item_itr = sessions().find(id.ToString());
    ASSERT_NE(sessions().end(), item_itr);

    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    item_itr->second.item()->MediaSessionMetadataChanged(std::move(metadata));
  }

  void SimulateMediaSessionActions(
      const base::UnguessableToken& id,
      const std::vector<media_session::mojom::MediaSessionAction>& actions) {
    auto item_itr = sessions().find(id.ToString());
    ASSERT_NE(sessions().end(), item_itr);

    item_itr->second.item()->MediaSessionActionsChanged(actions);
  }

  bool IsSessionFrozen(const base::UnguessableToken& id) const {
    auto item_itr = sessions().find(id.ToString());
    EXPECT_NE(sessions().end(), item_itr);
    return item_itr->second.item()->frozen();
  }

  bool IsSessionInactive(const base::UnguessableToken& id) const {
    return base::Contains(producer_->inactive_session_ids_, id.ToString());
  }

  bool HasActiveItems() const {
    return !producer_->GetActiveControllableItemIds().empty();
  }

  bool HasFrozenItems() const { return producer_->HasFrozenItems(); }

  void SimulateTabClosed(const base::UnguessableToken& id) {
    // When a tab is closing, audio focus will be lost before the WebContents is
    // destroyed, so to simulate closer to reality we will also simulate audio
    // focus lost here.
    SimulateFocusLost(id);

    // Now, close the tab. The session may have been destroyed with
    // |SimulateFocusLost()| above.
    producer_->OnRequestIdReleased(id);
  }

  void SimulatePlaybackStateChanged(const base::UnguessableToken& id,
                                    bool playing) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;
    session_info->playback_state =
        playing ? media_session::mojom::MediaPlaybackState::kPlaying
                : media_session::mojom::MediaPlaybackState::kPaused;
    SimulateMediaSessionInfoChanged(id, std::move(session_info));
  }

  void SimulateSessionHasPresentation(const base::UnguessableToken& id) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->has_presentation = true;
    SimulateMediaSessionInfoChanged(id, std::move(session_info));
  }

  void SimulateMediaSessionInfoChanged(const base::UnguessableToken& id,
                                       MediaSessionInfoPtr session_info) {
    auto item_itr = sessions().find(id.ToString());
    EXPECT_NE(sessions().end(), item_itr);
    item_itr->second.MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateMediaSeeked(const base::UnguessableToken& id) {
    auto item_itr = sessions().find(id.ToString());
    EXPECT_NE(sessions().end(), item_itr);
    item_itr->second.MediaSessionPositionChanged(std::nullopt);
  }

  void SimulateNotificationClicked(const base::UnguessableToken& id,
                                   bool activate_original_media) {
    producer_->OnMediaItemUIClicked(id.ToString(), activate_original_media);
  }

  void SimulateDismissButtonClicked(const base::UnguessableToken& id) {
    producer_->OnMediaItemUIDismissed(id.ToString());
  }

  void SimulateItemRefresh(const base::UnguessableToken& id) {
    producer_->RefreshItem(id.ToString());
  }

  void ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason reason,
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

  MediaSessionItemProducer::Session* GetSession(
      const base::UnguessableToken& id) {
    return producer_->GetSession(id.ToString());
  }

  std::map<std::string, MediaSessionItemProducer::Session>& sessions() const {
    return producer_->sessions_;
  }

  test::MockMediaItemManager& item_manager() { return item_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::MockMediaItemManager item_manager_;
  std::unique_ptr<media_session::test::MockAudioFocusManager>
      audio_focus_manager_;
  std::unique_ptr<media_session::test::MockMediaControllerManager>
      media_controller_manager_;
  std::unique_ptr<MediaSessionItemProducer> producer_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MediaSessionItemProducerTest, ShowControllableOnGainAndHideOnLoss) {
  // Simulate a new active, controllable media session.
  EXPECT_CALL(item_manager(), ShowItem(_));
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_FALSE(IsSessionFrozen(id));
  EXPECT_TRUE(HasActiveItems());

  // Ensure that the item manager was notified of the new item.
  testing::Mock::VerifyAndClearExpectations(&item_manager());

  // Simulate the active session ending.
  EXPECT_CALL(item_manager(), OnItemsChanged()).Times(AtLeast(1));
  EXPECT_FALSE(HasFrozenItems());
  SimulateFocusLost(id);

  // Ensure that the session was frozen and not hidden.
  EXPECT_TRUE(IsSessionFrozen(id));
  EXPECT_TRUE(HasFrozenItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());

  // Once the freeze timer fires, we should hide the media session.
  EXPECT_CALL(item_manager(), HideItem(id.ToString())).Times(AtLeast(1));
  AdvanceClockMilliseconds(2500);
  testing::Mock::VerifyAndClearExpectations(&item_manager());
}

TEST_F(MediaSessionItemProducerTest, DoesNotShowUncontrollableSession) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  // When focus is gained, we should not show an active session.
  EXPECT_FALSE(HasActiveItems());
  SimulateFocusGained(id, false);
  SimulateNecessaryMetadata(id);
  EXPECT_FALSE(HasActiveItems());

  // When focus is lost, we should not have a frozen session.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasFrozenItems());

  // When focus is regained, we should still not have an active session.
  SimulateFocusGained(id, false);
  EXPECT_FALSE(HasActiveItems());
}

TEST_F(MediaSessionItemProducerTest,
       DoesNotShowControllableSessionThatBecomesUncontrollable) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Lose focus so the item freezes.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // After 1s, the item should still be frozen.
  AdvanceClockMilliseconds(1000);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // If the item regains focus but is not controllable, it should not become
  // active.
  SimulateFocusGained(id, false);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // And the frozen timer should still fire after the initial 2.5 seconds is
  // finished.
  AdvanceClockMilliseconds(1400);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  AdvanceClockMilliseconds(200);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_FALSE(HasFrozenItems());
}

TEST_F(MediaSessionItemProducerTest, HideAfterTimeoutAndActiveAgainOnPlay) {
  // First, start an active session.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, stop playing media so the session is frozen, but not yet hidden.
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kMediaSessionStopped, 0);
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kMediaSessionStopped, 0);

  // If the time hasn't elapsed yet, the session should still be frozen.
  AdvanceClockMilliseconds(2400);
  EXPECT_TRUE(HasFrozenItems());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kMediaSessionStopped, 0);

  // Once the time is elapsed, the session should be hidden.
  EXPECT_CALL(item_manager(), HideItem(id.ToString())).Times(AtLeast(1));
  AdvanceClockMilliseconds(200);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_FALSE(HasFrozenItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kMediaSessionStopped, 1);

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(item_manager(), ShowItem(_)).Times(AtLeast(1));
  SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());
}

TEST_F(MediaSessionItemProducerTest,
       BecomesActiveIfMediaStartsPlayingWithinTimeout) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, stop playing media so the session is frozen, but hasn't been hidden
  // yet.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // If the time hasn't elapsed yet, we should still not be hidden.
  AdvanceClockMilliseconds(2400);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // If media starts playing again, we should become active again.
  EXPECT_CALL(item_manager(), ShowItem(_)).Times(AtLeast(1));
  SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());
}

TEST_F(MediaSessionItemProducerTest,
       SessionIsRemovedImmediatelyWhenATabCloses) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, close the tab. The session should immediately be hidden.
  EXPECT_CALL(item_manager(), OnItemsChanged()).Times(AtLeast(1));
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kTabClosed, 0);
  SimulateTabClosed(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_FALSE(HasFrozenItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kTabClosed, 1);
}

TEST_F(MediaSessionItemProducerTest, DismissesMediaSession) {
  // First, start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, click the dismiss button. This should stop and hide the session.
  EXPECT_CALL(item_manager(), HideItem(id.ToString())).Times(AtLeast(1));
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kUserDismissedNotification, 0);
  SimulateDismissButtonClicked(id);
  testing::Mock::VerifyAndClearExpectations(&item_manager());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kUserDismissedNotification, 1);
}

// Regression test for https://crbug.com/1015903: we could end up in a
// situation where the toolbar icon was disabled indefinitely.
TEST_F(MediaSessionItemProducerTest, LoseGainLoseDoesNotCauseRaceCondition) {
  // First, start an active session with some actions.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  std::vector<media_session::mojom::MediaSessionAction> actions = {
      MediaSessionAction::kPlay};
  SimulateMediaSessionActions(id, actions);
  EXPECT_TRUE(HasActiveItems());

  // Then, stop playing media so the session is frozen, but hasn't been hidden
  // yet.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // Simulate no actions, so we wait for actions.
  actions.clear();
  SimulateMediaSessionActions(id, actions);

  // Simulate gaining focus but with no actions yet so we wait.
  SimulateFocusGained(id, true);
  SimulateNecessaryMetadata(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // Then, lose focus again before getting actions.
  SimulateFocusLost(id);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(HasFrozenItems());

  // When the freeze timer fires, we should be hidden.
  EXPECT_CALL(item_manager(), HideItem(id.ToString())).Times(AtLeast(1));
  AdvanceClockMilliseconds(2600);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_FALSE(HasFrozenItems());
  testing::Mock::VerifyAndClearExpectations(&item_manager());
}

TEST_F(MediaSessionItemProducerTest, HidesInactiveNotifications) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveItems());

  // But once it's been inactive for over an hour, it should disappear.
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kInactiveTimeout, 0);
  AdvanceClockMinutes(2);
  EXPECT_FALSE(HasActiveItems());
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kInactiveTimeout, 1);

  // Since the user never interacted with the media before it was paused, we
  // should not have recorded any post-pause interactions.
  ExpectEmptyInteractionHistogram();

  // If we now close the tab, then it shouldn't record that as the dismiss
  // reason, since we already recorded a reason.
  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kTabClosed, 0);

  SimulateTabClosed(id);

  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kTabClosed, 0);
}

TEST_F(MediaSessionItemProducerTest, InactiveBecomesActive_PlayPause) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());
  EXPECT_FALSE(IsSessionInactive(id));

  // Let the notification become inactive.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(IsSessionInactive(id));

  ExpectHistogramInteractionDelayAfterPause(base::Minutes(70), 0);

  // Then, play the media. The notification should become active.
  SimulatePlaybackStateChanged(id, true);

  // We should have recorded an interaction even though the timer has
  // finished.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(70), 1);
  EXPECT_TRUE(HasActiveItems());
  EXPECT_FALSE(IsSessionInactive(id));
}

TEST_F(MediaSessionItemProducerTest, InactiveBecomesActive_Seeking) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());
  EXPECT_FALSE(IsSessionInactive(id));

  // Let the notification become inactive.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(IsSessionInactive(id));

  ExpectHistogramInteractionDelayAfterPause(base::Minutes(70), 0);

  // Then, seek the media. The notification should become active.
  SimulateMediaSeeked(id);

  // We should have recorded an interaction even though the timer has
  // finished.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(70), 1);
  EXPECT_TRUE(HasActiveItems());
  EXPECT_FALSE(IsSessionInactive(id));

  // If we don't interact again, the notification should become inactive
  // again.
  AdvanceClockMinutes(70);
  EXPECT_FALSE(HasActiveItems());
  EXPECT_TRUE(IsSessionInactive(id));
}

TEST_F(MediaSessionItemProducerTest, DelaysHidingNotifications_PlayPause) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveItems());

  // If we start playing again, we should not hide the notification, even
  // after an hour.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(59), 0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(59), 1);
  AdvanceClockMinutes(2);
  EXPECT_TRUE(HasActiveItems());

  // If we pause again, it should hide after an hour.
  SimulatePlaybackStateChanged(id, false);
  AdvanceClockMinutes(61);
  EXPECT_FALSE(HasActiveItems());
}

TEST_F(MediaSessionItemProducerTest, DelaysHidingNotifications_Interactions) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());

  // After 59 minutes, the notification should still be there.
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveItems());

  // If the user clicks to go back to the tab, it should reset the hide timer.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(59), 0);
  SimulateNotificationClicked(id, /*activate_original_media=*/true);
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(59), 1);
  AdvanceClockMinutes(50);
  EXPECT_TRUE(HasActiveItems());

  // If the user seeks the media before an hour is up, it should reset the
  // hide timer.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(50), 0);
  SimulateMediaSeeked(id);
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(50), 1);
  AdvanceClockMinutes(59);
  EXPECT_TRUE(HasActiveItems());

  // After the hour has passed, the notification should hide.
  AdvanceClockMinutes(2);
  EXPECT_FALSE(HasActiveItems());
}

TEST_F(MediaSessionItemProducerTest, HidingNotification_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kGlobalMediaControlsAutoDismiss);

  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());

  // After 61 minutes, the notification should still be there.
  AdvanceClockMinutes(61);
  EXPECT_TRUE(HasActiveItems());

  ExpectHistogramDismissReasonRecorded(
      GlobalMediaControlsDismissReason::kInactiveTimeout, 0);

  // Since the user never interacted with the media before it was paused, we
  // should not have recorded any post-pause interactions.
  ExpectEmptyInteractionHistogram();

  ExpectHistogramInteractionDelayAfterPause(base::Minutes(61), 0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(61), 1);
}

TEST_F(MediaSessionItemProducerTest, HidingNotification_TimerParams) {
  const int kTimerInMinutes = 6;
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["timer_in_minutes"] = base::NumberToString(kTimerInMinutes);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kGlobalMediaControlsAutoDismiss, params);

  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_TRUE(HasActiveItems());

  // Then, pause the media. We should still have the active notification.
  SimulatePlaybackStateChanged(id, false);
  EXPECT_TRUE(HasActiveItems());

  // After (kTimerInMinutes-1) minutes, the notification should still be
  // there.
  AdvanceClockMinutes(kTimerInMinutes - 1);
  EXPECT_TRUE(HasActiveItems());

  // If we start playing again, we should not hide the notification, even
  // after kTimerInMinutes.
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(kTimerInMinutes - 1),
                                            0);
  SimulatePlaybackStateChanged(id, true);
  ExpectHistogramInteractionDelayAfterPause(base::Minutes(kTimerInMinutes - 1),
                                            1);
  AdvanceClockMinutes(2);
  EXPECT_TRUE(HasActiveItems());

  // If we pause again, it should hide after kTimerInMinutes.
  SimulatePlaybackStateChanged(id, false);
  AdvanceClockMinutes(kTimerInMinutes + 1);
  EXPECT_FALSE(HasActiveItems());
}

TEST_F(MediaSessionItemProducerTest, RefreshSessionWhenRemotePlaybackChanges) {
  EXPECT_CALL(item_manager(), ShowItem(_));
  const base::UnguessableToken id = SimulatePlayingControllableMedia();
  EXPECT_CALL(item_manager(), RefreshItem(id.ToString()));
  SimulateItemRefresh(id);
}

TEST_F(MediaSessionItemProducerTest, ClicksNotificationItem) {
  // Start playing active media.
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  auto item_itr = sessions().find(id.ToString());
  ASSERT_NE(sessions().end(), item_itr);
  auto* item = item_itr->second.item();

  // Add a mock media controller for the notification item.
  auto test_media_controller =
      std::make_unique<media_session::test::TestMediaController>();
  MediaSessionInfoPtr session_info(MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  item->SetController(test_media_controller->CreateMediaControllerRemote(),
                      session_info.Clone());
  SimulateNecessaryMetadata(id);

  // Click the notification item without raising it.
  EXPECT_EQ(0, test_media_controller->raise_count());
  SimulateNotificationClicked(id, /*activate_original_media=*/false);
  item->FlushForTesting();
  EXPECT_EQ(0, test_media_controller->raise_count());

  // Click the notification item and raise it.
  SimulateNotificationClicked(id, /*activate_original_media=*/true);
  item->FlushForTesting();
  EXPECT_EQ(1, test_media_controller->raise_count());
}

}  // namespace global_media_controls
