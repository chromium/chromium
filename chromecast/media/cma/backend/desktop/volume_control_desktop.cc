// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/volume_control.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"

namespace chromecast {
namespace media {

namespace {

const float kMinVolumeDbfs = -60.0f;
const float kMaxVolumeDbfs = 0.0f;

class VolumeControlInternal {
 public:
  VolumeControlInternal() : thread_("VolumeControl") {
    for (auto type :
         {AudioContentType::kMedia, AudioContentType::kAlarm,
          AudioContentType::kCommunication, AudioContentType::kOther}) {
      volumes_[type] = 1.0f;
      muted_[type] = false;
    }

    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    thread_.StartWithOptions(std::move(options));
  }

  VolumeControlInternal(const VolumeControlInternal&) = delete;
  VolumeControlInternal& operator=(const VolumeControlInternal&) = delete;

  ~VolumeControlInternal() = default;

  void AddVolumeObserver(VolumeObserver* observer) {
    base::AutoLock lock(observer_lock_);
    volume_observers_.push_back(observer);
  }

  void RemoveVolumeObserver(VolumeObserver* observer) {
    base::AutoLock lock(observer_lock_);
    volume_observers_.erase(std::remove(volume_observers_.begin(),
                                        volume_observers_.end(), observer),
                            volume_observers_.end());
  }

  float GetVolume(AudioContentType type) {
    base::AutoLock lock(volume_lock_);
    return volumes_[type];
  }

  void SetVolume(media::VolumeChangeSource source,
                 AudioContentType type,
                 float level) {
    if (type == AudioContentType::kOther) {
      NOTREACHED() << "Can't set volume for content type kOther";
    }

    level = std::clamp(level, 0.0f, 1.0f);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VolumeControlInternal::SetVolumeOnThread,
                                  base::Unretained(this), source, type, level));
  }

  bool IsMuted(AudioContentType type) {
    base::AutoLock lock(volume_lock_);
    return muted_[type];
  }

  void SetMuted(media::VolumeChangeSource source,
                AudioContentType type,
                bool muted) {
    if (type == AudioContentType::kOther) {
      NOTREACHED() << "Can't set mute state for content type kOther";
    }

    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VolumeControlInternal::SetMutedOnThread,
                                  base::Unretained(this), source, type, muted));
  }

 private:
  void SetVolumeOnThread(media::VolumeChangeSource source,
                         AudioContentType type,
                         float level) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(type != AudioContentType::kOther);

    {
      base::AutoLock lock(volume_lock_);
      if (level == volumes_[type]) {
        return;
      }
      volumes_[type] = level;
    }

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnVolumeChange(source, type, level);
      }
    }
  }

  void SetMutedOnThread(media::VolumeChangeSource source,
                        AudioContentType type,
                        bool muted) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(type != AudioContentType::kOther);

    {
      base::AutoLock lock(volume_lock_);
      if (muted == muted_[type]) {
        return;
      }
      muted_[type] = muted;
    }

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnMuteChange(source, type, muted);
      }
    }
  }

  base::Lock volume_lock_;
  std::map<AudioContentType, float> volumes_;
  std::map<AudioContentType, bool> muted_;

  base::Lock observer_lock_;
  std::vector<VolumeObserver*> volume_observers_;

  base::Thread thread_;
};

VolumeControlInternal& GetVolumeControl() {
  static base::NoDestructor<VolumeControlInternal> g_volume_control;
  return *g_volume_control;
}

}  // namespace

// static
void VolumeControl::Initialize(const std::vector<std::string>& argv) {
  GetVolumeControl();
}

// static
void VolumeControl::Finalize() {}

// static
void VolumeControl::AddVolumeObserver(VolumeObserver* observer) {
  GetVolumeControl().AddVolumeObserver(observer);
}

// static
void VolumeControl::RemoveVolumeObserver(VolumeObserver* observer) {
  GetVolumeControl().RemoveVolumeObserver(observer);
}

// static
float VolumeControl::GetVolume(AudioContentType type) {
  return GetVolumeControl().GetVolume(type);
}

// static
void VolumeControl::SetVolume(media::VolumeChangeSource source,
                              AudioContentType type,
                              float level) {
  GetVolumeControl().SetVolume(source, type, level);
}

// static
bool VolumeControl::IsMuted(AudioContentType type) {
  return GetVolumeControl().IsMuted(type);
}

// static
void VolumeControl::SetMuted(media::VolumeChangeSource source,
                             AudioContentType type,
                             bool muted) {
  GetVolumeControl().SetMuted(source, type, muted);
}

// static
void VolumeControl::SetOutputLimit(AudioContentType type, float limit) {}

// static
float VolumeControl::VolumeToDbFS(float volume) {
  volume = std::clamp(volume, 0.0f, 1.0f);
  return kMinVolumeDbfs + volume * (kMaxVolumeDbfs - kMinVolumeDbfs);
}

// static
float VolumeControl::DbFSToVolume(float db) {
  db = std::clamp(db, kMinVolumeDbfs, kMaxVolumeDbfs);
  return (db - kMinVolumeDbfs) / (kMaxVolumeDbfs - kMinVolumeDbfs);
}

}  // namespace media
}  // namespace chromecast
