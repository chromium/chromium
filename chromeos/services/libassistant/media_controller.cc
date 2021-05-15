// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/media_controller.h"

#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/util_headers.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "libassistant/shared/public/media_manager.h"

namespace chromeos {
namespace libassistant {

namespace {

constexpr char kNextTrackClientOp[] = "media.NEXT";
constexpr char kPauseTrackClientOp[] = "media.PAUSE";
constexpr char kPlayMediaClientOp[] = "media.PLAY_MEDIA";
constexpr char kPrevTrackClientOp[] = "media.PREVIOUS";
constexpr char kResumeTrackClientOp[] = "media.RESUME";
constexpr char kStopTrackClientOp[] = "media.STOP";

constexpr char kIntentActionView[] = "android.intent.action.VIEW";

constexpr char kWebUrlPrefix[] = "http";

using chromeos::assistant::AndroidAppInfo;
using chromeos::assistant::shared::PlayMediaArgs;

// A macro which ensures we are running on the mojom thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

assistant_client::MediaStatus::PlaybackState ToPlaybackState(
    mojom::PlaybackState input) {
  switch (input) {
    case mojom::PlaybackState::kError:
      return assistant_client::MediaStatus::PlaybackState::ERROR;
    case mojom::PlaybackState::kIdle:
      return assistant_client::MediaStatus::PlaybackState::IDLE;
    case mojom::PlaybackState::kNewTrack:
      return assistant_client::MediaStatus::PlaybackState::NEW_TRACK;
    case mojom::PlaybackState::kPaused:
      return assistant_client::MediaStatus::PlaybackState::PAUSED;
    case mojom::PlaybackState::kPlaying:
      return assistant_client::MediaStatus::PlaybackState::PLAYING;
  }
}

assistant_client::MediaStatus ToMediaStatus(const mojom::MediaState& input) {
  assistant_client::MediaStatus result;

  if (input.metadata) {
    result.metadata.album = input.metadata->album;
    result.metadata.title = input.metadata->title;
    result.metadata.artist = input.metadata->artist;
  }
  result.playback_state = ToPlaybackState(input.playback_state);

  return result;
}

mojom::PlaybackState ToPlaybackState(
    assistant_client::MediaStatus::PlaybackState input) {
  switch (input) {
    case assistant_client::MediaStatus::PlaybackState::ERROR:
      return mojom::PlaybackState::kError;
    case assistant_client::MediaStatus::PlaybackState::IDLE:
      return mojom::PlaybackState::kIdle;
    case assistant_client::MediaStatus::PlaybackState::NEW_TRACK:
      return mojom::PlaybackState::kNewTrack;
    case assistant_client::MediaStatus::PlaybackState::PAUSED:
      return mojom::PlaybackState::kPaused;
    case assistant_client::MediaStatus::PlaybackState::PLAYING:
      return mojom::PlaybackState::kPlaying;
  }
}

mojom::MediaStatePtr ToMediaStatePtr(
    const assistant_client::MediaStatus& input) {
  mojom::MediaStatePtr result = mojom::MediaState::New();

  if (!input.metadata.album.empty() || !input.metadata.title.empty() ||
      !input.metadata.artist.empty()) {
    result->metadata = mojom::MediaMetadata::New();
    result->metadata->album = input.metadata.album;
    result->metadata->title = input.metadata.title;
    result->metadata->artist = input.metadata.artist;
  }
  result->playback_state = ToPlaybackState(input.playback_state);

  return result;
}

std::string GetAndroidIntentUrlFromMediaArgs(
    const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto media_item : play_media_args.media_item()) {
      if (!media_item.has_uri())
        continue;

      return media_item.uri();
    }
  }
  return std::string();
}

absl::optional<AndroidAppInfo> GetAppInfoFromMediaArgs(
    const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto& media_item : play_media_args.media_item()) {
      if (media_item.has_provider() &&
          media_item.provider().has_android_app_info()) {
        auto& app_info = media_item.provider().android_app_info();
        AndroidAppInfo result;
        result.package_name = app_info.package_name();
        result.version = app_info.app_version();
        result.localized_app_name = app_info.localized_app_name();
        result.intent = app_info.android_intent();
        return result;
      }
    }
  }
  return absl::nullopt;
}

std::string GetWebUrlFromMediaArgs(const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto media_item : play_media_args.media_item()) {
      if (!media_item.has_uri())
        continue;

      // For web url in browser.
      if (base::StartsWith(media_item.uri(), kWebUrlPrefix))
        return media_item.uri();
    }
  }

  return std::string();
}

}  // namespace

class MediaController::LibassistantMediaManagerListener
    : public assistant_client::MediaManager::Listener {
 public:
  explicit LibassistantMediaManagerListener(MediaController* parent)
      : parent_(parent),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  LibassistantMediaManagerListener(const LibassistantMediaManagerListener&) =
      delete;
  LibassistantMediaManagerListener& operator=(
      const LibassistantMediaManagerListener&) = delete;
  ~LibassistantMediaManagerListener() override = default;

  // assistant_client::MediaManager::Listener implementation:
  // Called from the Libassistant thread.
  void OnPlaybackStateChange(
      const assistant_client::MediaStatus& media_status) override {
    ENSURE_MOJOM_THREAD(
        &LibassistantMediaManagerListener::OnPlaybackStateChange, media_status);

    VLOG(1) << "Sending playback state update";
    delegate().OnPlaybackStateChanged(ToMediaStatePtr(media_status));
  }

 private:
  mojom::MediaDelegate& delegate() { return *parent_->delegate_; }

  MediaController* const parent_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<LibassistantMediaManagerListener> weak_factory_{this};
};

class MediaController::LibassistantMediaHandler {
 public:
  LibassistantMediaHandler(
      MediaController* parent,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      : parent_(parent),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    // Register handler for media actions.
    assistant_manager_internal->RegisterFallbackMediaHandler(
        [this](std::string action_name, std::string media_action_args_proto) {
          HandleMediaAction(action_name, media_action_args_proto);
        });
  }
  LibassistantMediaHandler(const LibassistantMediaHandler&) = delete;
  LibassistantMediaHandler& operator=(const LibassistantMediaHandler&) = delete;
  ~LibassistantMediaHandler() = default;

 private:
  // Called from the Libassistant thread.
  void HandleMediaAction(const std::string& action_name,
                         const std::string& media_action_args_proto) {
    ENSURE_MOJOM_THREAD(&LibassistantMediaHandler::HandleMediaAction,
                        action_name, media_action_args_proto);
    if (action_name == kPlayMediaClientOp)
      OnPlayMedia(media_action_args_proto);
    else
      OnMediaControlAction(action_name, media_action_args_proto);
  }

  void OnPlayMedia(const std::string& play_media_args_proto) {
    absl::optional<AndroidAppInfo> app_info =
        GetAppInfoFromMediaArgs(play_media_args_proto);
    if (app_info) {
      OnOpenMediaAndroidIntent(play_media_args_proto,
                               std::move(app_info.value()));
    } else {
      OnOpenUrl(play_media_args_proto);
    }
  }

  // Handle android media playback intent.
  void OnOpenMediaAndroidIntent(const std::string& play_media_args_proto,
                                AndroidAppInfo app_info) {
    app_info.action = kIntentActionView;
    if (app_info.intent.empty()) {
      std::string url = GetAndroidIntentUrlFromMediaArgs(play_media_args_proto);
      if (!url.empty())
        app_info.intent = url;
    }
    VLOG(1) << "Playing android media";
    delegate().PlayAndroidMedia(std::move(app_info));
  }

  void OnOpenUrl(const std::string& play_media_args_proto) {
    std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
    // Fallback to web URL.
    if (!url.empty()) {
      VLOG(1) << "Playing web media";
      delegate().PlayWebMedia(url);
    }
  }

  void OnMediaControlAction(const std::string& action_name,
                            const std::string& media_action_args_proto) {
    if (action_name == kPauseTrackClientOp) {
      VLOG(1) << "Pausing media playback";
      delegate().Pause();
      return;
    }

    if (action_name == kResumeTrackClientOp) {
      VLOG(1) << "Resuming media playback";
      delegate().Resume();
      return;
    }

    if (action_name == kNextTrackClientOp) {
      VLOG(1) << "Playing next track";
      delegate().NextTrack();
      return;
    }

    if (action_name == kPrevTrackClientOp) {
      VLOG(1) << "Playing previous track";
      delegate().PreviousTrack();
      return;
    }

    if (action_name == kStopTrackClientOp) {
      VLOG(1) << "Stop media playback";
      delegate().Stop();
      return;
    }
  }

  mojom::MediaDelegate& delegate() { return *parent_->delegate_; }

  MediaController* const parent_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<LibassistantMediaHandler> weak_factory_{this};
};

MediaController::MediaController()
    : listener_(std::make_unique<LibassistantMediaManagerListener>(this)) {}

MediaController::~MediaController() = default;

void MediaController::Bind(
    mojo::PendingReceiver<mojom::MediaController> receiver,
    mojo::PendingRemote<mojom::MediaDelegate> delegate) {
  receiver_.Bind(std::move(receiver));
  delegate_.Bind(std::move(delegate));
}

void MediaController::ResumeInternalMediaPlayer() {
  VLOG(1) << "Resume internal media player";
  if (media_manager())
    media_manager()->Resume();
}

void MediaController::PauseInternalMediaPlayer() {
  VLOG(1) << "Pause internal media player";
  if (media_manager())
    media_manager()->Pause();
}

void MediaController::SetExternalPlaybackState(mojom::MediaStatePtr state) {
  DCHECK(!state.is_null());
  VLOG(1) << "Update external playback state to " << state->playback_state;
  if (media_manager())
    media_manager()->SetExternalPlaybackState(ToMediaStatus(*state));
}

void MediaController::OnAssistantManagerRunning(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_ = assistant_manager;

  // Media manager should be created when Libassistant signals it is running.
  DCHECK(media_manager());

  handler_ = std::make_unique<LibassistantMediaHandler>(
      this, assistant_manager_internal);

  media_manager()->AddListener(listener_.get());
}

void MediaController::OnDestroyingAssistantManager(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_ = nullptr;
}

void MediaController::OnAssistantManagerDestroyed() {
  // Handler can only be unset after the |AssistantManagerInternal| has been
  // destroyed, as |AssistantManagerInternal| will call the handler.
  handler_ = nullptr;
}

assistant_client::MediaManager* MediaController::media_manager() {
  return assistant_manager_ ? assistant_manager_->GetMediaManager() : nullptr;
}

}  // namespace libassistant
}  // namespace chromeos
