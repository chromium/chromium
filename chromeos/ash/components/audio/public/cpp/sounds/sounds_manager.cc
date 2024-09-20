// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/audio/public/cpp/sounds/audio_stream_handler.h"
#include "media/base/audio_codecs.h"

namespace audio {

namespace {

SoundsManager* g_instance = NULL;
bool g_initialized_for_testing = false;

// SoundsManagerImpl ---------------------------------------------------

class SoundsManagerImpl : public SoundsManager {
 public:
  explicit SoundsManagerImpl(StreamFactoryBinder stream_factory_binder)
      : stream_factory_binder_(std::move(stream_factory_binder)) {}

  SoundsManagerImpl(const SoundsManagerImpl&) = delete;
  SoundsManagerImpl& operator=(const SoundsManagerImpl&) = delete;

  ~SoundsManagerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // SoundsManager implementation:
  bool Initialize(SoundKey key,
                  std::string_view data,
                  media::AudioCodec codec) override;
  bool Play(SoundKey key) override;
  bool Stop(SoundKey key) override;
  base::TimeDelta GetDuration(SoundKey key) override;

 private:
  AudioStreamHandler* GetHandler(SoundKey key);

  // There's only a handful of sounds, so a vector is sufficient.
  struct StreamEntry {
    SoundKey key;
    std::unique_ptr<AudioStreamHandler> handler;
  };
  std::vector<StreamEntry> handlers_;
  StreamFactoryBinder stream_factory_binder_;
};

bool SoundsManagerImpl::Initialize(SoundKey key,
                                   std::string_view data,
                                   media::AudioCodec codec) {
  if (AudioStreamHandler* handler = GetHandler(key)) {
    DCHECK(handler->IsInitialized());
    return true;
  }

  std::unique_ptr<AudioStreamHandler> handler(
      new AudioStreamHandler(stream_factory_binder_, data, codec));
  if (!handler->IsInitialized()) {
    LOG(WARNING) << "Can't initialize AudioStreamHandler for key=" << key;
    return false;
  }

  handlers_.push_back({key, std::move(handler)});
  return true;
}

bool SoundsManagerImpl::Play(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  return handler && handler->Play();
}

bool SoundsManagerImpl::Stop(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  if (!handler) {
    return false;
  }
  handler->Stop();
  return true;
}

base::TimeDelta SoundsManagerImpl::GetDuration(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  return !handler ? base::TimeDelta() : handler->duration();
}

AudioStreamHandler* SoundsManagerImpl::GetHandler(SoundKey key) {
  for (auto& entry : handlers_) {
    if (entry.key == key) {
      return entry.handler.get();
    }
  }
  return nullptr;
}

}  // namespace

SoundsManager::SoundsManager() = default;

SoundsManager::~SoundsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SoundsManager::Create(StreamFactoryBinder stream_factory_binder) {
  CHECK(!g_instance || g_initialized_for_testing)
      << "SoundsManager::Create() is called twice";
  if (g_initialized_for_testing) {
    return;
  }
  g_instance = new SoundsManagerImpl(std::move(stream_factory_binder));
}

// static
void SoundsManager::Shutdown() {
  CHECK(g_instance) << "SoundsManager::Shutdown() is called "
                    << "without previous call to Create()";
  delete g_instance;
  g_instance = NULL;
}

// static
SoundsManager* SoundsManager::Get() {
  CHECK(g_instance) << "SoundsManager::Get() is called before Create()";
  return g_instance;
}

// static
void SoundsManager::InitializeForTesting(SoundsManager* manager) {
  CHECK(!g_instance) << "SoundsManager is already initialized.";
  CHECK(manager);
  g_instance = manager;
  g_initialized_for_testing = true;
}

}  // namespace audio
