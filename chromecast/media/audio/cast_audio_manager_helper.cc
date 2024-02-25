// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_helper.h"

#include <utility>

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"

namespace chromecast {
namespace media {

CastAudioManagerHelper::CastAudioManagerHelper(
    ::media::AudioManagerBase* audio_manager,
    Delegate* delegate,
    base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : audio_manager_(audio_manager),
      delegate_(delegate),
      backend_factory_getter_(std::move(backend_factory_getter)),
      media_task_runner_(std::move(media_task_runner)) {
  DCHECK(audio_manager_);
  DCHECK(delegate_);
  DCHECK(backend_factory_getter_);
}

CastAudioManagerHelper::~CastAudioManagerHelper() = default;

CmaBackendFactory* CastAudioManagerHelper::GetCmaBackendFactory() {
  if (!cma_backend_factory_)
    cma_backend_factory_ = backend_factory_getter_.Run();
  return cma_backend_factory_;
}

std::string CastAudioManagerHelper::GetSessionId(
    const std::string& audio_group_id) {
  return delegate_->GetSessionId(audio_group_id);
}

bool CastAudioManagerHelper::IsAudioOnlySession(const std::string& session_id) {
  return delegate_->IsAudioOnlySession(session_id);
}

bool CastAudioManagerHelper::IsGroup(const std::string& session_id) {
  return delegate_->IsGroup(session_id);
}

}  // namespace media
}  // namespace chromecast
