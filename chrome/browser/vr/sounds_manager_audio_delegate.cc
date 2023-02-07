// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/sounds_manager_audio_delegate.h"
#include "content/public/browser/audio_service.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

namespace vr {

SoundsManagerAudioDelegate::SoundsManagerAudioDelegate() {}

SoundsManagerAudioDelegate::~SoundsManagerAudioDelegate() {
  ResetSounds();
}

void SoundsManagerAudioDelegate::ResetSounds() {
  // Because SoundsManager cannot replace a registered sound, start fresh
  // with a new manager if needed.
  if (!sounds_.empty()) {
    audio::SoundsManager::Shutdown();
    sounds_.clear();
  }
}

bool SoundsManagerAudioDelegate::RegisterSound(
    SoundId id,
    std::unique_ptr<std::string> data) {
  DCHECK_NE(id, kSoundNone);
  DCHECK(sounds_.find(id) == sounds_.end());

  if (sounds_.empty())
    audio::SoundsManager::Create(content::GetAudioServiceStreamFactoryBinder());

  sounds_[id] = std::move(data);
  return audio::SoundsManager::Get()->Initialize(id, *sounds_[id],
                                                 media::AudioCodec::kPCM);
}

void SoundsManagerAudioDelegate::PlaySound(SoundId id) {
  if (sounds_.find(id) != sounds_.end())
    audio::SoundsManager::Get()->Play(id);
}

}  // namespace vr
