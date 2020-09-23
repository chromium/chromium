// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/cma_backend_proxy.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {
namespace {

// The maximum allowed difference between the audio and video decoders used for
// the CmaBackendProxy.
// TODO(b/168748626): Determine the correct value for this variable
// experimentally.
int64_t kMaxAllowedPtsDrift = 500;

}  // namespace

CmaBackendProxy::CmaBackendProxy(
    std::unique_ptr<CmaBackend> delegated_video_pipeline)
    : CmaBackendProxy(
          std::move(delegated_video_pipeline),
          base::BindOnce([]() -> std::unique_ptr<MultizoneAudioDecoderProxy> {
            return std::make_unique<MultizoneAudioDecoderProxyImpl>();
          })) {}

CmaBackendProxy::CmaBackendProxy(
    std::unique_ptr<CmaBackend> delegated_video_pipeline,
    CmaBackendProxy::AudioDecoderFactoryCB audio_decoder_factory)
    : delegated_video_pipeline_(std::move(delegated_video_pipeline)),
      audio_decoder_factory_(std::move(audio_decoder_factory)) {
  DCHECK(delegated_video_pipeline_);
  DCHECK(audio_decoder_factory_);
}

CmaBackendProxy::~CmaBackendProxy() = default;

CmaBackend::AudioDecoder* CmaBackendProxy::CreateAudioDecoder() {
  DCHECK(!audio_decoder_);
  DCHECK(audio_decoder_factory_);
  audio_decoder_ = std::move(audio_decoder_factory_).Run();
  return audio_decoder_.get();
}

CmaBackend::VideoDecoder* CmaBackendProxy::CreateVideoDecoder() {
  has_video_decoder_ = true;
  return delegated_video_pipeline_->CreateVideoDecoder();
}

bool CmaBackendProxy::Initialize() {
  if (has_video_decoder_ && !delegated_video_pipeline_->Initialize()) {
    return false;
  }

  return !audio_decoder_ || audio_decoder_->Initialize();
}

bool CmaBackendProxy::Start(int64_t start_pts) {
  if (has_video_decoder_ && !delegated_video_pipeline_->Start(start_pts)) {
    return false;
  }

  return !audio_decoder_ || audio_decoder_->Start(start_pts);
}

void CmaBackendProxy::Stop() {
  if (has_video_decoder_) {
    delegated_video_pipeline_->Stop();
  }

  if (audio_decoder_) {
    audio_decoder_->Stop();
  }
}

bool CmaBackendProxy::Pause() {
  bool result = true;

  if (has_video_decoder_) {
    result &= delegated_video_pipeline_->Pause();
  }

  if (audio_decoder_) {
    result &= audio_decoder_->Pause();
  }

  return result;
}

bool CmaBackendProxy::Resume() {
  if (has_video_decoder_ && !delegated_video_pipeline_->Resume()) {
    return false;
  }

  return !audio_decoder_ || audio_decoder_->Resume();
}

int64_t CmaBackendProxy::GetCurrentPts() {
  if (audio_decoder_ && has_video_decoder_) {
    const int64_t audio_pts = audio_decoder_->GetCurrentPts();
    const int64_t video_pts = delegated_video_pipeline_->GetCurrentPts();
    const int64_t min = std::min(audio_pts, video_pts);
    LOG_IF(WARNING, std::max(audio_pts, video_pts) - min > kMaxAllowedPtsDrift);
    return min;
  } else if (audio_decoder_) {
    return audio_decoder_->GetCurrentPts();
  } else if (has_video_decoder_) {
    return delegated_video_pipeline_->GetCurrentPts();
  } else {
    return std::numeric_limits<int64_t>::min();
  }
}

bool CmaBackendProxy::SetPlaybackRate(float rate) {
  bool result = true;

  if (has_video_decoder_) {
    result &= delegated_video_pipeline_->SetPlaybackRate(rate);
  }

  if (audio_decoder_) {
    result &= audio_decoder_->SetPlaybackRate(rate);
  }

  return result;
}

void CmaBackendProxy::LogicalPause() {
  if (has_video_decoder_) {
    delegated_video_pipeline_->LogicalPause();
  }

  if (audio_decoder_) {
    audio_decoder_->LogicalPause();
  }
}

void CmaBackendProxy::LogicalResume() {
  if (has_video_decoder_) {
    delegated_video_pipeline_->LogicalResume();
  }

  if (audio_decoder_) {
    audio_decoder_->LogicalResume();
  }
}

}  // namespace media
}  // namespace chromecast
