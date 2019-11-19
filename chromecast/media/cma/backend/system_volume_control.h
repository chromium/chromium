// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_SYSTEM_VOLUME_CONTROL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_SYSTEM_VOLUME_CONTROL_H_

#include <memory>

#include "base/macros.h"

namespace chromecast {
namespace media {

// Handles setting the volume and mute state on the appropriate system mixer
// elements (based on command-line args); also detects changes to the mute state
// or volume and informs a delegate.
// Must be created on an IO thread, and all methods must be called on that
// thread.
class SystemVolumeControl {
 public:
  class Delegate {
   public:
    // Called whenever the system volume or mute state have changed.
    // Unfortunately it is not possible in all cases to differentiate between
    // a volume change and a mute change, so the two events must be combined.
    virtual void OnSystemVolumeOrMuteChange(float new_volume,
                                            bool new_mute) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  static std::unique_ptr<SystemVolumeControl> Create(Delegate* delegate);

  SystemVolumeControl() = default;
  virtual ~SystemVolumeControl() = default;

  // Returns the value that you would get if you called GetVolume() after
  // SetVolume(volume).
  virtual float GetRoundtripVolume(float volume) = 0;

  // Returns the current system volume (0 <= volume <= 1).
  virtual float GetVolume() = 0;

  // Sets the system volume to |level| (0 <= level <= 1).
  virtual void SetVolume(float level) = 0;

  // Returns |true| if system is currently muted.
  virtual bool IsMuted() = 0;

  // Sets the system mute state to |muted|.
  virtual void SetMuted(bool muted) = 0;

  // Sets the system power save state to |power_save_on|.
  virtual void SetPowerSave(bool power_save_on) = 0;

  // Sets the volume limit to be applied to the system volume.
  virtual void SetLimit(float limit) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemVolumeControl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_SYSTEM_VOLUME_CONTROL_H_
