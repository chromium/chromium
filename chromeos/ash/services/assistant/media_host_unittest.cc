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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

using libassistant::mojom::MediaState;
using libassistant::mojom::MediaStatePtr;
using libassistant::mojom::PlaybackState;
using media_session::mojom::MediaSessionInfo;
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

class MediaControllerMock : public media_session::mojom::MediaController {
 public:
  // media_session::mojom::MediaController implementation:
  MOCK_METHOD(void, Suspend, ());
  MOCK_METHOD(void, Resume, ());
  MOCK_METHOD(void, Stop, ());
  MOCK_METHOD(void, ToggleSuspendResume, ());
  MOCK_METHOD(void, PreviousTrack, ());
  MOCK_METHOD(void, NextTrack, ());
  MOCK_METHOD(void, Seek, (::base::TimeDelta seek_time));
  MOCK_METHOD(
      void,
      ObserveImages,
      (::media_session::mojom::MediaSessionImageType type,
       int32_t minimum_size_px,
       int32_t desired_size_px,
       mojo::PendingRemote<media_session::mojom::MediaControllerImageObserver>
           observer));
  MOCK_METHOD(void, SeekTo, (::base::TimeDelta seek_time));
  MOCK_METHOD(void, ScrubTo, (::base::TimeDelta seek_time));
  MOCK_METHOD(void, EnterPictureInPicture, ());
  MOCK_METHOD(void, ExitPictureInPicture, ());
  MOCK_METHOD(void, SetAudioSinkId, (const absl::optional<std::string>& id));
  MOCK_METHOD(void, ToggleMicrophone, ());
  MOCK_METHOD(void, ToggleCamera, ());
  MOCK_METHOD(void, HangUp, ());
  MOCK_METHOD(void, Raise, ());
  MOCK_METHOD(void, SetMute, (bool mute));
  MOCK_METHOD(void, RequestMediaRemoting, ());

  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaControllerObserver> remote)
      override {
    observer_.Bind(std::move(remote));
  }

  void Bind(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  mojo::Remote<media_session::mojom::MediaControllerObserver>& observer() {
    return observer_;
  }

 private:
  mojo::Remote<media_session::mojom::MediaControllerObserver> observer_;
  mojo::Receiver<media_session::mojom::MediaController> receiver_{this};
};

class FakeMediaControllerManager
    : public media_session::mojom::MediaControllerManager {
 public:
  FakeMediaControllerManager() = default;
  FakeMediaControllerManager(const FakeMediaControllerManager&) = delete;
  FakeMediaControllerManager& operator=(const FakeMediaControllerManager&) =
      delete;
  ~FakeMediaControllerManager() override = default;

  mojo::Receiver<media_session::mojom::MediaControllerManager>& receiver() {
    return receiver_;
  }

  MediaControllerMock& media_controller_mock() { return media_controller_; }

  // media_session::mojom::MediaControllerManager implementation:
  void CreateMediaControllerForSession(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver,
      const ::base::UnguessableToken& request_id) override {
    NOTIMPLEMENTED();
  }
  void CreateActiveMediaController(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver)
      override {
    media_controller_.Bind(std::move(receiver));
  }
  void SuspendAllSessions() override { NOTIMPLEMENTED(); }

 private:
  mojo::Receiver<media_session::mojom::MediaControllerManager> receiver_{this};
  testing::StrictMock<MediaControllerMock> media_controller_;
};

class MediaSessionObserverMock
    : public media_session::mojom::MediaSessionObserver {
 public:
  MediaSessionObserverMock() = default;
  MediaSessionObserverMock(const MediaSessionObserverMock&) = delete;
  MediaSessionObserverMock& operator=(const MediaSessionObserverMock&) = delete;
  ~MediaSessionObserverMock() override = default;

  // media_session::mojom::MediaSessionObserver implementation:
  MOCK_METHOD(void,
              MediaSessionInfoChanged,
              (media_session::mojom::MediaSessionInfoPtr info));
  MOCK_METHOD(void,
              MediaSessionMetadataChanged,
              (const absl::optional<::media_session::MediaMetadata>& metadata));
  MOCK_METHOD(
      void,
      MediaSessionActionsChanged,
      (const std::vector<media_session::mojom::MediaSessionAction>& action));
  MOCK_METHOD(void,
              MediaSessionImagesChanged,
              ((const base::flat_map<
                  media_session::mojom::MediaSessionImageType,
                  std::vector<::media_session::MediaImage>>& images)));
  MOCK_METHOD(void,
              MediaSessionPositionChanged,
              (const absl::optional<::media_session::MediaPosition>& position));

  mojo::PendingRemote<media_session::mojom::MediaSessionObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<media_session::mojom::MediaSessionObserver> receiver_{this};
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

  MediaControllerMock& chromeos_media_controller_mock() {
    return media_controller_manager_.media_controller_mock();
  }

  mojo::Remote<media_session::mojom::MediaControllerObserver>&
  chromeos_media_controller_observer() {
    return chromeos_media_controller_mock().observer();
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
    chromeos_media_controller_observer()->MediaSessionChanged(token);
    chromeos_media_controller_observer().FlushForTesting();
  }

  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) {
    chromeos_media_controller_observer()->MediaSessionInfoChanged(
        std::move(session_info));
    chromeos_media_controller_observer().FlushForTesting();
  }

  void MediaSessionMetadataChanged(
      const media_session::MediaMetadata& meta_data) {
    chromeos_media_controller_observer()->MediaSessionMetadataChanged(
        meta_data);
    chromeos_media_controller_observer().FlushForTesting();
  }

  void AddMediaSessionObserver(MediaSessionObserverMock& observer) {
    // The observer is always called when adding.
    EXPECT_CALL(observer, MediaSessionMetadataChanged);
    EXPECT_CALL(observer, MediaSessionInfoChanged);
    media_session().AddObserver(observer.BindNewPipeAndPassRemote());
    observer.FlushForTesting();
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

TEST_F(MediaHostTest, ShouldInitiallyNotObserverMediaChanges) {
  EXPECT_FALSE(chromeos_media_controller_observer().is_bound());
}

TEST_F(MediaHostTest,
       ShouldStartObservingMediaChangesWhenRelatedInfoIsEnabled) {
  SetRelatedInfoEnabled(true);

  EXPECT_TRUE(chromeos_media_controller_observer().is_bound());
  EXPECT_TRUE(chromeos_media_controller_observer().is_connected());
}

TEST_F(MediaHostTest,
       ShouldStopObservingMediaChangesAndFlushStateWhenRelatedInfoIsDisabled) {
  SetRelatedInfoEnabled(true);

  EXPECT_CALL(libassistant_controller_mock(), SetExternalPlaybackState);
  SetRelatedInfoEnabled(false);

  // Note the observer is not unbound, but the connection is severed.
  EXPECT_FALSE(chromeos_media_controller_observer().is_connected());
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
  media_host().media_session().SetInternalAudioFocusIdForTesting(session_id);

  EXPECT_NO_CALLS(libassistant_controller_mock(), SetExternalPlaybackState);

  media_session::MediaMetadata meta_data;
  MediaSessionMetadataChanged(meta_data);
}

TEST_F(MediaHostTest, ShouldForwardLibassistantMediaSessionUpdates) {
  testing::StrictMock<MediaSessionObserverMock> media_session_observer;
  AddMediaSessionObserver(media_session_observer);

  absl::optional<media_session::MediaMetadata> expected_output =
      media_session::MediaMetadata();
  expected_output->title = u"the title";
  expected_output->artist = u"the artist";
  expected_output->album = u"the album";
  EXPECT_CALL(media_session_observer,
              MediaSessionMetadataChanged(expected_output));

  auto input = MediaState::New();
  input->metadata = libassistant::mojom::MediaMetadata::New();
  input->metadata->title = "the title";
  input->metadata->artist = "the artist";
  input->metadata->album = "the album";
  libassistant_media_delegate().OnPlaybackStateChanged(std::move(input));

  FlushMojomPipes();
  media_session_observer.FlushForTesting();
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
  EXPECT_CALL(chromeos_media_controller_mock(), NextTrack);

  libassistant_media_delegate().NextTrack();

  FlushMojomPipes();
}

TEST_F(MediaHostTest, ShouldPlayPreviousTrack) {
  EXPECT_CALL(chromeos_media_controller_mock(), PreviousTrack);

  libassistant_media_delegate().PreviousTrack();

  FlushMojomPipes();
}

TEST_F(MediaHostTest, ShouldPause) {
  EXPECT_CALL(chromeos_media_controller_mock(), Suspend);

  libassistant_media_delegate().Pause();

  FlushMojomPipes();
}

TEST_F(MediaHostTest, ShouldResume) {
  EXPECT_CALL(chromeos_media_controller_mock(), Resume);

  libassistant_media_delegate().Resume();

  FlushMojomPipes();
}

TEST_F(MediaHostTest, ShouldStop) {
  EXPECT_CALL(chromeos_media_controller_mock(), Suspend);

  libassistant_media_delegate().Stop();

  FlushMojomPipes();
}

}  // namespace ash::assistant
