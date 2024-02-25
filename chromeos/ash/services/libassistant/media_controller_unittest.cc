// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/media_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/grpc/utils/media_status_utils.h"
#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/fake_assistant_client.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/util_headers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

using LibassistantPlaybackState = assistant_client::MediaStatus::PlaybackState;
using ProtoAndroidAppInfo = chromeos::assistant::shared::AndroidAppInfo;
using assistant::AndroidAppInfo;
using chromeos::assistant::shared::PlayMediaArgs;
using mojom::PlaybackState;

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

std::string MediaStatusToString(
    const assistant_client::MediaStatus& media_status) {
  return base::StringPrintf(R"(
          MediaStatus {
              playback_state '%i'
              metadata.album '%s'
              metadata.artist '%s'
              metadata.title '%s'
          )",
                            static_cast<int>(media_status.playback_state),
                            media_status.metadata.album.c_str(),
                            media_status.metadata.artist.c_str(),
                            media_status.metadata.title.c_str());
}

MATCHER_P(MatchesMediaStatus, expected, "") {
  if (MediaStatusToString(arg) == MediaStatusToString(expected))
    return true;

  *result_listener << "\nExpected: " << MediaStatusToString(expected);
  *result_listener << "\nActual: " << MediaStatusToString(arg);
  return false;
}

class MediaDelegateMock : public mojom::MediaDelegate {
 public:
  MediaDelegateMock() = default;
  MediaDelegateMock(const MediaDelegateMock&) = delete;
  MediaDelegateMock& operator=(const MediaDelegateMock&) = delete;
  ~MediaDelegateMock() override = default;

  mojo::PendingRemote<mojom::MediaDelegate> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // mojom::MediaDelegate implementation:
  MOCK_METHOD(void, OnPlaybackStateChanged, (mojom::MediaStatePtr new_state));
  MOCK_METHOD(void, PlayAndroidMedia, (const AndroidAppInfo& app_info));
  MOCK_METHOD(void, PlayWebMedia, (const std::string& url));
  MOCK_METHOD(void, NextTrack, ());
  MOCK_METHOD(void, PreviousTrack, ());
  MOCK_METHOD(void, Pause, ());
  MOCK_METHOD(void, Resume, ());
  MOCK_METHOD(void, Stop, ());

 private:
  mojo::Receiver<mojom::MediaDelegate> receiver_{this};
};

class MediaManagerMock : public assistant_client::MediaManager {
 public:
  MediaManagerMock() = default;
  MediaManagerMock(const MediaManagerMock&) = delete;
  MediaManagerMock& operator=(const MediaManagerMock&) = delete;
  ~MediaManagerMock() override = default;

  // assistant_client::MediaManager implementation:
  void AddListener(Listener* listener) override { listener_ = listener; }
  MOCK_METHOD(void, Next, ());
  MOCK_METHOD(void, Previous, ());
  MOCK_METHOD(void, Resume, ());
  MOCK_METHOD(void, Pause, ());
  MOCK_METHOD(void, PlayPause, ());
  MOCK_METHOD(void, StopAndClearPlaylist, ());
  MOCK_METHOD(void,
              SetExternalPlaybackState,
              (const assistant_client::MediaStatus& new_status));

  Listener& listener() {
    DCHECK(listener_);
    return *listener_;
  }

 private:
  raw_ptr<Listener> listener_ = nullptr;
};

}  // namespace

class AssistantMediaControllerTest : public testing::Test {
 public:
  AssistantMediaControllerTest()
      : media_controller_(std::make_unique<MediaController>()) {
    media_controller_->Bind(client_.BindNewPipeAndPassReceiver(),
                            delegate_.BindNewPipeAndPassRemote());
  }

  void SetUp() override {
    service_tester_.Start();
    service_tester_.assistant_manager().SetMediaManager(&media_manager_);
    media_controller_->OnAssistantClientRunning(&assistant_client());
  }

  MediaManagerMock& libassistant_media_manager() { return media_manager_; }

  mojo::Remote<mojom::MediaController>& client() { return client_; }

  MediaDelegateMock& delegate() { return delegate_; }

  MediaController& media_controller() {
    DCHECK(media_controller_);
    return *media_controller_;
  }

  void SendPlaybackState(const assistant_client::MediaStatus& input) {
    ::assistant::api::OnDeviceStateEventRequest request;
    auto* status = request.mutable_event()
                       ->mutable_on_state_changed()
                       ->mutable_new_state()
                       ->mutable_media_status();
    ConvertMediaStatusToV2FromV1(input, status);
    media_controller().SendGrpcMessageForTesting(request);
  }

  void CallFallbackMediaHandler(const std::string& action,
                                const std::string& action_proto) {
    ::assistant::api::OnMediaActionFallbackEventRequest request;
    auto* media_action =
        request.mutable_event()->mutable_on_media_action_event();
    media_action->set_action_name(action);
    media_action->set_action_args(action_proto);
    media_controller().SendGrpcMessageForTesting(request);
  }

  void FlushMojomPipes() {
    delegate_.FlushForTesting();
    client_.FlushForTesting();
    service_tester_.FlushForTesting();
  }

  void RemoveAssistantManager() {
    media_controller_->OnDestroyingAssistantClient(&assistant_client());
  }

 private:
  AssistantClient& assistant_client() {
    return service_tester_.assistant_client();
  }

  base::test::SingleThreadTaskEnvironment environment_;

  MediaManagerMock media_manager_;
  mojo::Remote<mojom::MediaController> client_;
  testing::StrictMock<MediaDelegateMock> delegate_;
  std::unique_ptr<MediaController> media_controller_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AssistantMediaControllerTest,
       ShouldSendResumeToLibassistantMediaPlayer) {
  EXPECT_CALL(libassistant_media_manager(), Resume);

  media_controller().ResumeInternalMediaPlayer();
}

TEST_F(AssistantMediaControllerTest, ShouldSendPauseToLibassistantMediaPlayer) {
  EXPECT_CALL(libassistant_media_manager(), Pause);

  media_controller().PauseInternalMediaPlayer();
}

TEST_F(AssistantMediaControllerTest, ShouldSendMediaStatusToLibassistant) {
  EXPECT_CALL(libassistant_media_manager(), SetExternalPlaybackState);

  auto input = mojom::MediaState::New();
  media_controller().SetExternalPlaybackState(std::move(input));
}

TEST_F(AssistantMediaControllerTest, ShouldSendMediaMetadataToLibassistant) {
  assistant_client::MediaStatus expected;
  expected.metadata.album = "album";
  expected.metadata.artist = "artist";
  expected.metadata.title = "title";
  EXPECT_CALL(libassistant_media_manager(),
              SetExternalPlaybackState(MatchesMediaStatus(expected)));

  auto input = mojom::MediaState::New();
  input->metadata = mojom::MediaMetadata::New();
  input->metadata->album = "album";
  input->metadata->artist = "artist";
  input->metadata->title = "title";
  media_controller().SetExternalPlaybackState(std::move(input));
}

TEST_F(AssistantMediaControllerTest,
       ShouldSendMediaPlaybackStateToLibassistant) {
  std::vector<std::pair<PlaybackState, LibassistantPlaybackState>> pairs = {
      {PlaybackState::kError, LibassistantPlaybackState::ERROR},
      {PlaybackState::kIdle, LibassistantPlaybackState::IDLE},
      {PlaybackState::kPaused, LibassistantPlaybackState::PAUSED},
      {PlaybackState::kNewTrack, LibassistantPlaybackState::NEW_TRACK},
      {PlaybackState::kPlaying, LibassistantPlaybackState::PLAYING},
  };

  for (auto pair : pairs) {
    assistant_client::MediaStatus expected;
    expected.playback_state = pair.second;
    EXPECT_CALL(libassistant_media_manager(),
                SetExternalPlaybackState(MatchesMediaStatus(expected)));

    auto input = mojom::MediaState::New();
    input->playback_state = pair.first;
    media_controller().SetExternalPlaybackState(std::move(input));

    testing::Mock::VerifyAndClearExpectations(&libassistant_media_manager());
  }
}

TEST_F(AssistantMediaControllerTest,
       ShouldNotCrashIfAssistantManagerIsNotPresent) {
  RemoveAssistantManager();

  media_controller().ResumeInternalMediaPlayer();
  media_controller().PauseInternalMediaPlayer();
  media_controller().SetExternalPlaybackState(mojom::MediaState::New());
}

TEST_F(AssistantMediaControllerTest, ShouldSendPlaybackStateChangeToDelegate) {
  mojom::MediaStatePtr actual;
  EXPECT_CALL(delegate(), OnPlaybackStateChanged)
      .WillOnce([&](mojom::MediaStatePtr state) { actual = std::move(state); });

  assistant_client::MediaStatus input;
  input.metadata.album = "album";
  input.metadata.artist = "artist";
  input.metadata.title = "title";
  SendPlaybackState(input);
  FlushMojomPipes();

  ASSERT_FALSE(actual.is_null());
  ASSERT_FALSE(actual->metadata.is_null());
  EXPECT_EQ(actual->metadata->album, "album");
  EXPECT_EQ(actual->metadata->artist, "artist");
  EXPECT_EQ(actual->metadata->title, "title");
}

TEST_F(AssistantMediaControllerTest, ShouldSendPlaybackStateToDelegate) {
  std::vector<std::pair<PlaybackState, LibassistantPlaybackState>> pairs = {
      {PlaybackState::kError, LibassistantPlaybackState::ERROR},
      {PlaybackState::kIdle, LibassistantPlaybackState::IDLE},
      {PlaybackState::kPaused, LibassistantPlaybackState::PAUSED},
      {PlaybackState::kNewTrack, LibassistantPlaybackState::NEW_TRACK},
      {PlaybackState::kPlaying, LibassistantPlaybackState::PLAYING},
  };

  for (auto pair : pairs) {
    mojom::MediaStatePtr actual;
    EXPECT_CALL(delegate(), OnPlaybackStateChanged)
        .WillOnce(
            [&](mojom::MediaStatePtr state) { actual = std::move(state); });

    assistant_client::MediaStatus input;
    input.playback_state = pair.second;
    SendPlaybackState(input);
    FlushMojomPipes();

    ASSERT_FALSE(actual.is_null());
    EXPECT_EQ(actual->playback_state, pair.first);
  }
}

TEST_F(AssistantMediaControllerTest, ShouldSupportNext) {
  EXPECT_CALL(delegate(), NextTrack);
  CallFallbackMediaHandler("media.NEXT", "");
  FlushMojomPipes();
}

TEST_F(AssistantMediaControllerTest, ShouldSupportPrevious) {
  EXPECT_CALL(delegate(), PreviousTrack);
  CallFallbackMediaHandler("media.PREVIOUS", "");
  FlushMojomPipes();
}

TEST_F(AssistantMediaControllerTest, ShouldSupportPause) {
  EXPECT_CALL(delegate(), Pause);
  CallFallbackMediaHandler("media.PAUSE", "");
  FlushMojomPipes();
}

TEST_F(AssistantMediaControllerTest, ShouldSupportResume) {
  EXPECT_CALL(delegate(), Resume);
  CallFallbackMediaHandler("media.RESUME", "");
  FlushMojomPipes();
}

TEST_F(AssistantMediaControllerTest, ShouldSupportStop) {
  EXPECT_CALL(delegate(), Stop);
  CallFallbackMediaHandler("media.STOP", "");
  FlushMojomPipes();
}

TEST_F(AssistantMediaControllerTest, ShouldSupportPlayAndroidMedia) {
  PlayMediaArgs play_media_args;
  PlayMediaArgs::MediaItem* media_item = play_media_args.add_media_item();
  ProtoAndroidAppInfo* android_app_info =
      media_item->mutable_provider()->mutable_android_app_info();
  android_app_info->set_package_name("package");
  android_app_info->set_localized_app_name("app name");
  android_app_info->set_app_version(111);
  media_item->set_uri("http://the/uri");

  std::optional<AndroidAppInfo> actual;
  EXPECT_CALL(delegate(), PlayAndroidMedia)
      .WillOnce([&](const AndroidAppInfo& a) { actual = a; });

  CallFallbackMediaHandler("media.PLAY_MEDIA",
                           play_media_args.SerializeAsString());
  FlushMojomPipes();

  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(actual.value().package_name, "package");
  EXPECT_EQ(actual.value().localized_app_name, "app name");
  EXPECT_EQ(actual.value().version, 111);
  EXPECT_EQ(actual.value().action, "android.intent.action.VIEW");
  EXPECT_EQ(actual.value().intent, "http://the/uri");
}

TEST_F(AssistantMediaControllerTest, ShouldSupportPlayWebMedia) {
  PlayMediaArgs play_media_args;
  PlayMediaArgs::MediaItem* media_item = play_media_args.add_media_item();
  media_item->set_uri("http://the/url");

  std::string actual_url = "<not-called>";
  EXPECT_CALL(delegate(), PlayWebMedia).WillOnce([&](std::string url) {
    actual_url = url;
  });

  CallFallbackMediaHandler("media.PLAY_MEDIA",
                           play_media_args.SerializeAsString());
  FlushMojomPipes();

  EXPECT_EQ(actual_url, "http://the/url");
}

TEST_F(AssistantMediaControllerTest, ShouldIgnoreInvalidUrls) {
  PlayMediaArgs play_media_args;
  PlayMediaArgs::MediaItem* media_item = play_media_args.add_media_item();
  media_item->set_uri("not-a-url");

  EXPECT_NO_CALLS(delegate(), PlayWebMedia);

  CallFallbackMediaHandler("media.PLAY_MEDIA",
                           play_media_args.SerializeAsString());
  FlushMojomPipes();
}
}  // namespace ash::libassistant
