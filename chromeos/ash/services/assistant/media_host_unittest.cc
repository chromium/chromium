// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/media_host.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/test_support/libassistant_media_controller_mock.h"
#include "chromeos/ash/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "chromeos/ash/services/libassistant/public/mojom/android_app_info.mojom-shared.h"
#include "chromeos/ash/services/libassistant/public/mojom/android_app_info.mojom.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::assistant {

namespace {

using libassistant::mojom::MediaState;
using libassistant::mojom::MediaStatePtr;
using libassistant::mojom::PlaybackState;
using media_session::mojom::MediaSessionInfo;
using media_session::test::MockMediaSessionMojoObserver;
using media_session::test::TestMediaController;
using ::testing::_;

constexpr char kPlayAndroidMediaAction[] = "android.intent.action.VIEW";

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

MATCHER_P(PlaybackStateIs, expected_state, "") {
  if (arg.is_null()) {
    *result_listener << "MediaStatePtr is nullptr";
    return false;
  }

  if (arg->playback_state != expected_state) {
    *result_listener << "Expected " << expected_state << " but got "
                     << arg->playback_state;
    return false;
  }
  return true;
}

std::string AndroidAppInfoToString(const AndroidAppInfo& app_info) {
  return base::StringPrintf(R"(
          AndroidAppInfo {
              package_name '%s'
              version '%i'
              localized_app_name '%s'
              action '%s'
              intent '%s'
              status '%i'
          )",
                            app_info.package_name.c_str(), app_info.version,
                            app_info.localized_app_name.c_str(),
                            app_info.action.c_str(), app_info.intent.c_str(),
                            static_cast<int>(app_info.status));
}

MATCHER_P(MatchesAndroidAppInfo, expected, "") {
  if (AndroidAppInfoToString(arg) == AndroidAppInfoToString(expected))
    return true;

  *result_listener << "\nExpected: " << AndroidAppInfoToString(expected);
  *result_listener << "\nActual: " << AndroidAppInfoToString(arg);
  return false;
}

class FakeMediaControllerManager
    : public media_session::mojom::MediaControllerManager {
 public:
  FakeMediaControllerManager() {
    media_controller_ = std::make_unique<TestMediaController>();
  }
  FakeMediaControllerManager(const FakeMediaControllerManager&) = delete;
  FakeMediaControllerManager& operator=(const FakeMediaControllerManager&) =
      delete;
  ~FakeMediaControllerManager() override = default;

  mojo::Receiver<media_session::mojom::MediaControllerManager>& receiver() {
    return receiver_;
  }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  // media_session::mojom::MediaControllerManager implementation:
  void CreateMediaControllerForSession(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver,
      const ::base::UnguessableToken& request_id) override {
    NOTIMPLEMENTED();
  }
  void CreateActiveMediaController(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver)
      override {
    media_controller_->BindMediaControllerReceiver(std::move(receiver));
  }
  void SuspendAllSessions() override { NOTIMPLEMENTED(); }

 private:
  mojo::Receiver<media_session::mojom::MediaControllerManager> receiver_{this};
  std::unique_ptr<TestMediaController> media_controller_;
};

}  // namespace

class MediaHostTest : public testing::Test {
 public:
  MediaHostTest() = default;
  MediaHostTest(const MediaHostTest&) = delete;
  MediaHostTest& operator=(const MediaHostTest&) = delete;
  ~MediaHostTest() override = default;

  void SetUp() override {
    delegate_.SetMediaControllerManager(&media_controller_manager_.receiver());

    media_host_ = std::make_unique<MediaHost>(AssistantBrowserDelegate::Get(),
                                              &interaction_subscribers_);
    media_host().Initialize(
        &libassistant_controller_,
        libassistant_media_delegate_.BindNewPipeAndPassReceiver());
  }

  LibassistantMediaControllerMock& libassistant_controller_mock() {
    return libassistant_controller_;
  }

  libassistant::mojom::MediaDelegate& libassistant_media_delegate() {
    return *libassistant_media_delegate_;
  }

  MediaHost& media_host() { return *media_host_; }

  AssistantMediaSession& media_session() {
    return media_host().media_session();
  }

  TestMediaController* media_controller() const {
    return media_controller_manager_.media_controller();
  }

  void FlushMojomPipes() {
    media_controller_manager_.receiver().FlushForTesting();
    libassistant_media_delegate_.FlushForTesting();
  }

  void SetRelatedInfoEnabled(bool enabled) {
    media_host().SetRelatedInfoEnabled(enabled);
    FlushMojomPipes();
  }

  void StartMediaSession(
      base::UnguessableToken token = base::UnguessableToken::Create()) {
    media_controller()->SimulateMediaSessionChanged(token);
    media_controller()->Flush();
  }

  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) {
    media_controller()->SimulateMediaSessionInfoChanged(
        std::move(session_info));
    media_controller()->Flush();
  }

  void MediaSessionMetadataChanged(
      const media_session::MediaMetadata& meta_data) {
    media_controller()->SimulateMediaSessionMetadataChanged(meta_data);
    media_controller()->Flush();
  }

  void AddAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) {
    interaction_subscribers_.AddObserver(subscriber);
  }

  void ClearAssistantInteractionSubscribers() {
    interaction_subscribers_.Clear();
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  base::ObserverList<AssistantInteractionSubscriber> interaction_subscribers_;
  FakeMediaControllerManager media_controller_manager_;
  ScopedAssistantBrowserDelegate delegate_;
  testing::StrictMock<LibassistantMediaControllerMock> libassistant_controller_;
  mojo::Remote<libassistant::mojom::MediaDelegate> libassistant_media_delegate_;
  std::unique_ptr<MediaHost> media_host_;
};

TEST_F(MediaHostTest, ShouldSupportResumePlaying) {
  EXPECT_CALL(libassistant_controller_mock(), ResumeInternalMediaPlayer);

  media_host().ResumeInternalMediaPlayer();
}

TEST_F(MediaHostTest, ShouldSupportPausePlaying) {
  EXPECT_CALL(libassistant_controller_mock(), PauseInternalMediaPlayer);

  media_host().PauseInternalMediaPlayer();
}

TEST_F(MediaHostTest, ShouldInitiallyNotObserveMediaChanges) {
  EXPECT_EQ(0, media_controller()->add_observer_count());
}

TEST_F(MediaHostTest,
       ShouldStartObservingMediaChangesWhenRelatedInfoIsEnabled) {
  SetRelatedInfoEnabled(true);
  EXPECT_EQ(1, media_controller()->add_observer_count());
  EXPECT_EQ(1, media_controller()->GetActiveObserverCount());
}

TEST_F(MediaHostTest,
       ShouldStopObservingMediaChangesAndFlushStateWhenRelatedInfoIsDisabled) {
  SetRelatedInfoEnabled(true);

  EXPECT_CALL(libassistant_controller_mock(), SetExternalPlaybackState);
  SetRelatedInfoEnabled(false);

  // Note the observer is not unbound, but the connection is severed.
  EXPECT_EQ(1, media_controller()->add_observer_count());
  EXPECT_EQ(0, media_controller()->GetActiveObserverCount());
}

TEST_F(MediaHostTest, ShouldSetTitleWhenCallingSetExternalPlaybackState) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  MediaStatePtr actual_state;
  EXPECT_CALL(libassistant_controller_mock(), SetExternalPlaybackState)
      .WillOnce([&](MediaStatePtr state) { actual_state = std::move(state); });

  media_session::MediaMetadata meta_data;
  meta_data.title = u"the title";
  MediaSessionMetadataChanged(meta_data);

  ASSERT_FALSE(actual_state.is_null());
  ASSERT_FALSE(actual_state->metadata.is_null());
  EXPECT_EQ(actual_state->metadata->title, "the title");
}

TEST_F(MediaHostTest, ShouldDropSensitiveSessions) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  auto session_info = MediaSessionInfo::New();
  session_info->is_sensitive = true;
  MediaSessionInfoChanged(std::move(session_info));

  EXPECT_NO_CALLS(libassistant_controller_mock(), SetExternalPlaybackState);

  media_session::MediaMetadata meta_data;
  MediaSessionMetadataChanged(meta_data);
}

TEST_F(MediaHostTest, ShouldDropInvalidStates) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  auto session_info = MediaSessionInfo::New();
  session_info->state = MediaSessionInfo::SessionState::kSuspended;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  MediaSessionInfoChanged(std::move(session_info));

  EXPECT_NO_CALLS(libassistant_controller_mock(), SetExternalPlaybackState);

  media_session::MediaMetadata meta_data;
  MediaSessionMetadataChanged(meta_data);
}

TEST_F(MediaHostTest, ShouldSetPlaybackState) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  // State is idle if MediaSessionInfo is never set.
  {
    EXPECT_CALL(
        libassistant_controller_mock(),
        SetExternalPlaybackState(PlaybackStateIs(PlaybackState::kIdle)));
    media_session::MediaMetadata meta_data;
    MediaSessionMetadataChanged(meta_data);
  }

  // State kPlaying.
  {
    EXPECT_CALL(
        libassistant_controller_mock(),
        SetExternalPlaybackState(PlaybackStateIs(PlaybackState::kPlaying)));

    auto session_info = MediaSessionInfo::New();
    session_info->playback_state =
        media_session::mojom::MediaPlaybackState::kPlaying;
    MediaSessionInfoChanged(std::move(session_info));
  }

  // State kPaused.
  {
    EXPECT_CALL(
        libassistant_controller_mock(),
        SetExternalPlaybackState(PlaybackStateIs(PlaybackState::kPaused)));

    auto session_info = MediaSessionInfo::New();
    session_info->playback_state =
        media_session::mojom::MediaPlaybackState::kPaused;
    MediaSessionInfoChanged(std::move(session_info));
  }

  // State is kInvalid if session is inactive
  {
    EXPECT_CALL(
        libassistant_controller_mock(),
        SetExternalPlaybackState(PlaybackStateIs(PlaybackState::kIdle)));

    auto session_info = MediaSessionInfo::New();
    session_info->state = MediaSessionInfo::SessionState::kInactive;
    session_info->playback_state =
        media_session::mojom::MediaPlaybackState::kPaused;
    MediaSessionInfoChanged(std::move(session_info));
  }
}

TEST_F(MediaHostTest, ShouldResetPlaybackStateWhenDisablingRelatedInfo) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  MediaStatePtr actual_state;
  EXPECT_CALL(libassistant_controller_mock(), SetExternalPlaybackState)
      .WillOnce([&](MediaStatePtr state) { actual_state = std::move(state); });

  SetRelatedInfoEnabled(false);

  const MediaStatePtr empty_state = MediaState::New();
  EXPECT_EQ(actual_state, empty_state);
}

TEST_F(MediaHostTest,
       ShouldIgnorePlaybackStateUpdatesWhenRelatedInfoIsDisabled) {
  SetRelatedInfoEnabled(true);
  StartMediaSession();

  // Playback state is updated when disabling related info.
  EXPECT_CALL(libassistant_controller_mock(), SetExternalPlaybackState);
  SetRelatedInfoEnabled(false);

  // But not for any consecutive changes.
  EXPECT_NO_CALLS(libassistant_controller_mock(), SetExternalPlaybackState);
  media_session::MediaMetadata meta_data;
  MediaSessionMetadataChanged(meta_data);
}

TEST_F(MediaHostTest,
       ShouldIgnorePlaybackStateUpdatesForLibassistantInternalSessions) {
  SetRelatedInfoEnabled(true);

  const auto session_id = base::UnguessableToken::Create();
  StartMediaSession(session_id);
  media_session().SetInternalAudioFocusIdForTesting(session_id);

  EXPECT_NO_CALLS(libassistant_controller_mock(), SetExternalPlaybackState);

  media_session::MediaMetadata meta_data;
  MediaSessionMetadataChanged(meta_data);
}

TEST_F(MediaHostTest, ShouldForwardLibassistantMediaSessionUpdates) {
  MockMediaSessionMojoObserver media_session_observer(media_session());
  media_session_observer.WaitForEmptyMetadata();

  auto input = MediaState::New();
  input->metadata = libassistant::mojom::MediaMetadata::New();
  input->metadata->title = "the title";
  input->metadata->artist = "the artist";
  input->metadata->album = "the album";
  libassistant_media_delegate().OnPlaybackStateChanged(std::move(input));
  FlushMojomPipes();

  media_session::MediaMetadata expected_output;
  expected_output.title = u"the title";
  expected_output.artist = u"the artist";
  expected_output.album = u"the album";
  media_session_observer.WaitForExpectedMetadata(expected_output);
}

TEST_F(MediaHostTest, ShouldForwardLibassistantOpenAndroidMediaUpdates) {
  testing::StrictMock<MockAssistantInteractionSubscriber> mock;
  AddAssistantInteractionSubscriber(&mock);

  AndroidAppInfo output_app_info;
  output_app_info.package_name = "the package name";
  output_app_info.version = 111;
  output_app_info.localized_app_name = "the localized name";
  output_app_info.action = kPlayAndroidMediaAction;
  output_app_info.intent = "the intent";
  output_app_info.status = AppStatus::kUnknown;

  EXPECT_CALL(mock, OnOpenAppResponse(MatchesAndroidAppInfo(output_app_info)));

  AndroidAppInfo input_app_info;
  input_app_info.package_name = "the package name";
  input_app_info.version = 111;
  input_app_info.localized_app_name = "the localized name";
  input_app_info.intent = "the intent";
  input_app_info.status = AppStatus::kUnknown;
  input_app_info.action = kPlayAndroidMediaAction;

  libassistant_media_delegate().PlayAndroidMedia(std::move(input_app_info));

  FlushMojomPipes();
  ClearAssistantInteractionSubscribers();
}

TEST_F(MediaHostTest, ShouldOpenWebMediaUrl) {
  testing::StrictMock<MockAssistantInteractionSubscriber> mock;
  AddAssistantInteractionSubscriber(&mock);

  EXPECT_CALL(
      mock, OnOpenUrlResponse(GURL("http://the.url"), /*in_background=*/false));

  libassistant_media_delegate().PlayWebMedia("http://the.url");

  FlushMojomPipes();
  ClearAssistantInteractionSubscribers();
}

TEST_F(MediaHostTest, ShouldPlayNextTrack) {
  EXPECT_EQ(0, media_controller()->next_track_count());
  libassistant_media_delegate().NextTrack();
  FlushMojomPipes();
  EXPECT_EQ(1, media_controller()->next_track_count());
}

TEST_F(MediaHostTest, ShouldPlayPreviousTrack) {
  EXPECT_EQ(0, media_controller()->previous_track_count());
  libassistant_media_delegate().PreviousTrack();
  FlushMojomPipes();
  EXPECT_EQ(1, media_controller()->previous_track_count());
}

TEST_F(MediaHostTest, ShouldPause) {
  EXPECT_EQ(0, media_controller()->suspend_count());
  libassistant_media_delegate().Pause();
  FlushMojomPipes();
  EXPECT_EQ(1, media_controller()->suspend_count());
}

TEST_F(MediaHostTest, ShouldResume) {
  EXPECT_EQ(0, media_controller()->resume_count());
  libassistant_media_delegate().Resume();
  FlushMojomPipes();
  EXPECT_EQ(1, media_controller()->resume_count());
}

TEST_F(MediaHostTest, ShouldStop) {
  EXPECT_EQ(0, media_controller()->suspend_count());
  libassistant_media_delegate().Stop();
  FlushMojomPipes();
  EXPECT_EQ(1, media_controller()->suspend_count());
}

}  // namespace ash::assistant
