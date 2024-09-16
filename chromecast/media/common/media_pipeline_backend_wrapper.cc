// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/media_pipeline_backend_wrapper.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chromecast/media/common/audio_decoder_wrapper.h"
#include "chromecast/media/common/media_pipeline_backend_manager.h"
#include "chromecast/media/common/video_decoder_wrapper.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace {

std::unique_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  LOG(INFO) << "Beginning creation of MediaPipelineBackend...";
  auto backend = base::WrapUnique(
      media::CastMediaShlib::CreateMediaPipelineBackend(params));
  LOG(INFO) << "Completed creation of MediaPipelineBackend!";
  return backend;
}

}  // namespace

using DecoderType = MediaPipelineBackendManager::DecoderType;

// DecoderCreatorCmaBackend transfers the ownership of the created Decoders.
class DecoderCreatorCmaBackend : public CmaBackend {
 public:
  virtual std::unique_ptr<AudioDecoderWrapper> CreateAudioDecoderWrapper() = 0;
  virtual std::unique_ptr<VideoDecoderWrapper> CreateVideoDecoderWrapper() = 0;
};

namespace {

class RevokedMediaPipelineBackendWrapper : public DecoderCreatorCmaBackend {
 public:
  RevokedMediaPipelineBackendWrapper(const AudioContentType& content_type,
                                     int64_t current_pts)
      : content_type_(content_type), current_pts_(current_pts) {}

  RevokedMediaPipelineBackendWrapper(
      const RevokedMediaPipelineBackendWrapper&) = delete;
  RevokedMediaPipelineBackendWrapper& operator=(
      const RevokedMediaPipelineBackendWrapper&) = delete;

  ~RevokedMediaPipelineBackendWrapper() override = default;

  std::unique_ptr<AudioDecoderWrapper> CreateAudioDecoderWrapper() override {
    return std::make_unique<AudioDecoderWrapper>(content_type_);
  }

  std::unique_ptr<VideoDecoderWrapper> CreateVideoDecoderWrapper() override {
    return std::make_unique<VideoDecoderWrapper>();
  }

  // CmaBackend implementation:
  CmaBackend::AudioDecoder* CreateAudioDecoder() override { NOTREACHED(); }

  CmaBackend::VideoDecoder* CreateVideoDecoder() override { NOTREACHED(); }

  bool Initialize() override { return true; }
  bool Start(int64_t start_pts) override { return true; }
  void Stop() override {}
  bool Pause() override { return true; }
  bool Resume() override { return true; }
  int64_t GetCurrentPts() override { return current_pts_; }
  bool SetPlaybackRate(float rate) override { return true; }
  void LogicalPause() override {}
  void LogicalResume() override {}

 private:
  const AudioContentType content_type_;
  const int64_t current_pts_;
};

}  // namespace

class ActiveMediaPipelineBackendWrapper : public DecoderCreatorCmaBackend {
 public:
  ActiveMediaPipelineBackendWrapper(
      const media::MediaPipelineDeviceParams& params,
      MediaPipelineBackendWrapper* wrapping_backend,
      MediaPipelineBackendManager* backend_manager,
      MediaResourceTracker* media_resource_tracker);

  ActiveMediaPipelineBackendWrapper(const ActiveMediaPipelineBackendWrapper&) =
      delete;
  ActiveMediaPipelineBackendWrapper& operator=(
      const ActiveMediaPipelineBackendWrapper&) = delete;

  ~ActiveMediaPipelineBackendWrapper() override;

  // DecoderCreatorCmaBackend implementation:
  // Audio/VideoDecoders are owned by the MediaPipelineBackendWrapper.
  std::unique_ptr<AudioDecoderWrapper> CreateAudioDecoderWrapper() override;
  std::unique_ptr<VideoDecoderWrapper> CreateVideoDecoderWrapper() override;

  // CmaBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;
  void LogicalPause() override;
  void LogicalResume() override;

 private:
  void SetPlaying(bool playing);

  bool IsSfx() {
    return audio_stream_type_ ==
           media::MediaPipelineDeviceParams::kAudioStreamSoundEffects;
  }

  AudioDecoderWrapper* audio_decoder_ptr_;
  bool video_decoder_created_;
  const std::unique_ptr<MediaPipelineBackend> backend_;
  MediaPipelineBackendWrapper* const wrapping_backend_;
  MediaPipelineBackendManager* const backend_manager_;
  const MediaPipelineDeviceParams::AudioStreamType audio_stream_type_;
  const AudioContentType content_type_;

  // Acquire the media resource at construction. The resource will be released
  // when this class is destructed.
  MediaResourceTracker::ScopedUsage media_resource_usage_;

  bool playing_;
};

ActiveMediaPipelineBackendWrapper::ActiveMediaPipelineBackendWrapper(
    const media::MediaPipelineDeviceParams& params,
    MediaPipelineBackendWrapper* wrapping_backend,
    MediaPipelineBackendManager* backend_manager,
    MediaResourceTracker* media_resource_tracker)
    : audio_decoder_ptr_(nullptr),
      video_decoder_created_(false),
      backend_(CreateMediaPipelineBackend(params)),
      wrapping_backend_(wrapping_backend),
      backend_manager_(backend_manager),
      audio_stream_type_(params.audio_type),
      content_type_(params.content_type),
      media_resource_usage_(media_resource_tracker),
      playing_(false) {
  DCHECK(backend_);
  DCHECK(backend_manager_);
}

ActiveMediaPipelineBackendWrapper::~ActiveMediaPipelineBackendWrapper() {
  // When the backend is revoked,  the video/audio_decoder should be considered
  // gone to |backend_manager_|. The reason is that the replacement of the
  // Audio/VideoDecoderWrapper are dummy ones that are not actually playing.
  if (audio_decoder_ptr_) {
    backend_manager_->DecrementDecoderCount(
        IsSfx() ? DecoderType::SFX_DECODER : DecoderType::AUDIO_DECODER);
    if (playing_) {
      backend_manager_->UpdatePlayingAudioCount(IsSfx(), content_type_, -1);
    }
  }
  if (video_decoder_created_) {
    backend_manager_->DecrementDecoderCount(DecoderType::VIDEO_DECODER);
  }
}

CmaBackend::AudioDecoder*
ActiveMediaPipelineBackendWrapper::CreateAudioDecoder() {
  NOTREACHED();
}

CmaBackend::VideoDecoder*
ActiveMediaPipelineBackendWrapper::CreateVideoDecoder() {
  NOTREACHED();
}

void ActiveMediaPipelineBackendWrapper::LogicalPause() {
  SetPlaying(false);
}

void ActiveMediaPipelineBackendWrapper::LogicalResume() {
  SetPlaying(true);
}

std::unique_ptr<AudioDecoderWrapper>
ActiveMediaPipelineBackendWrapper::CreateAudioDecoderWrapper() {
  DCHECK(!audio_decoder_ptr_);

  if (!backend_manager_->IncrementDecoderCount(
          IsSfx() ? DecoderType::SFX_DECODER : DecoderType::AUDIO_DECODER))
    return nullptr;
  MediaPipelineBackend::AudioDecoder* real_decoder =
      backend_->CreateAudioDecoder();
  if (!real_decoder) {
    return nullptr;
  }

  MediaPipelineBackendManager::BufferDelegate* delegate = nullptr;
  // Only set delegate for the primary media stream.
  if (content_type_ == media::AudioContentType::kMedia &&
      audio_stream_type_ ==
          media::MediaPipelineDeviceParams::kAudioStreamNormal) {
    delegate = backend_manager_->buffer_delegate();
  }

  auto audio_decoder = std::make_unique<AudioDecoderWrapper>(
      real_decoder, content_type_, delegate);
  audio_decoder_ptr_ = audio_decoder.get();
  return audio_decoder;
}

std::unique_ptr<VideoDecoderWrapper>
ActiveMediaPipelineBackendWrapper::CreateVideoDecoderWrapper() {
  DCHECK(!video_decoder_created_);
  backend_manager_->BackendUseVideoDecoder(wrapping_backend_);

  if (!backend_manager_->IncrementDecoderCount(DecoderType::VIDEO_DECODER))
    return nullptr;

  MediaPipelineBackend::VideoDecoder* real_decoder =
      backend_->CreateVideoDecoder();
  if (!real_decoder) {
    return nullptr;
  }

  video_decoder_created_ = true;
  auto video_decoder = std::make_unique<VideoDecoderWrapper>(real_decoder);
  return video_decoder;
}

bool ActiveMediaPipelineBackendWrapper::Initialize() {
  LOG(INFO) << "Beginning initialization of MediaPipelineBackend...";
  bool success = backend_->Initialize();
  if (success && audio_decoder_ptr_) {
    audio_decoder_ptr_->OnInitialized();
  }
  LOG(INFO) << "Initialization of MediaPipelineBackend "
            << (success ? "succeeded!" : "failed!");
  return success;
}

bool ActiveMediaPipelineBackendWrapper::Start(int64_t start_pts) {
  if (!backend_->Start(start_pts)) {
    return false;
  }
  SetPlaying(true);
  return true;
}

void ActiveMediaPipelineBackendWrapper::Stop() {
  backend_->Stop();
  SetPlaying(false);
}

bool ActiveMediaPipelineBackendWrapper::Pause() {
  if (!backend_->Pause()) {
    return false;
  }
  SetPlaying(false);
  return true;
}

bool ActiveMediaPipelineBackendWrapper::Resume() {
  if (!backend_->Resume()) {
    return false;
  }
  SetPlaying(true);
  return true;
}

int64_t ActiveMediaPipelineBackendWrapper::GetCurrentPts() {
  return backend_->GetCurrentPts();
}

bool ActiveMediaPipelineBackendWrapper::SetPlaybackRate(float rate) {
  return backend_->SetPlaybackRate(rate);
}

void ActiveMediaPipelineBackendWrapper::SetPlaying(bool playing) {
  if (playing == playing_) {
    return;
  }
  playing_ = playing;
  if (audio_decoder_ptr_) {
    backend_manager_->UpdatePlayingAudioCount(IsSfx(), content_type_,
                                              (playing_ ? 1 : -1));
  }
}

MediaPipelineBackendWrapper::MediaPipelineBackendWrapper(
    const media::MediaPipelineDeviceParams& params,
    MediaPipelineBackendManager* backend_manager,
    MediaResourceTracker* media_resource_tracker)
    : revoked_(false),
      backend_manager_(backend_manager),
      content_type_(params.content_type) {
  backend_ = std::make_unique<ActiveMediaPipelineBackendWrapper>(
      params, this, backend_manager, media_resource_tracker);
}

MediaPipelineBackendWrapper::~MediaPipelineBackendWrapper() {
  backend_manager_->BackendDestroyed(this);
}

void MediaPipelineBackendWrapper::Revoke() {
  if (!revoked_) {
    revoked_ = true;
    if (audio_decoder_)
      audio_decoder_->Revoke();
    if (video_decoder_)
      video_decoder_->Revoke();

    backend_ = std::make_unique<RevokedMediaPipelineBackendWrapper>(
        content_type_, backend_->GetCurrentPts());
  }
}

CmaBackend::AudioDecoder* MediaPipelineBackendWrapper::CreateAudioDecoder() {
  LOG(INFO) << "Beginning creation of AudioDecoder...";
  DCHECK(!audio_decoder_);
  audio_decoder_ = backend_->CreateAudioDecoderWrapper();
  LOG(INFO) << "Completed creation of AudioDecoder!";
  return audio_decoder_.get();
}

CmaBackend::VideoDecoder* MediaPipelineBackendWrapper::CreateVideoDecoder() {
  LOG(INFO) << "Beginning creation of VideoDecoder...";
  DCHECK(!video_decoder_);
  video_decoder_ = backend_->CreateVideoDecoderWrapper();
  LOG(INFO) << "VideoDecoder creation of AudioDecoder!";
  return video_decoder_.get();
}

bool MediaPipelineBackendWrapper::Initialize() {
  return backend_->Initialize();
}
bool MediaPipelineBackendWrapper::Start(int64_t start_pts) {
  return backend_->Start(start_pts);
}
void MediaPipelineBackendWrapper::Stop() {
  backend_->Stop();
}
bool MediaPipelineBackendWrapper::Pause() {
  return backend_->Pause();
}
bool MediaPipelineBackendWrapper::Resume() {
  return backend_->Resume();
}
int64_t MediaPipelineBackendWrapper::GetCurrentPts() {
  return backend_->GetCurrentPts();
}
bool MediaPipelineBackendWrapper::SetPlaybackRate(float rate) {
  return backend_->SetPlaybackRate(rate);
}
void MediaPipelineBackendWrapper::LogicalPause() {
  return backend_->LogicalPause();
}
void MediaPipelineBackendWrapper::LogicalResume() {
  return backend_->LogicalResume();
}

}  // namespace media
}  // namespace chromecast
