// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/media/cma/backend/system_volume_control.h"

namespace chromecast {
namespace media {

class DesktopSystemVolumeControl : public SystemVolumeControl {
 public:
  explicit DesktopSystemVolumeControl(Delegate* delegate)
      : delegate_(delegate) {}
  ~DesktopSystemVolumeControl() override = default;

  // SystemVolumeControl interface.
  // Consider 'level' and 'volume' equivalent for simplicity.
  float GetRoundtripVolume(float volume) override { return volume; }
  float GetVolume() override { return volume_; }
  void SetVolume(float level) override {
    if (level != volume_) {
      volume_ = level;
      delegate_->OnSystemVolumeOrMuteChange(volume_, muted_);
    }
  }
  bool IsMuted() override { return muted_; }
  void SetMuted(bool muted) override {
    if (muted != muted_) {
      muted_ = muted;
      delegate_->OnSystemVolumeOrMuteChange(volume_, muted_);
    }
  }
  void SetPowerSave(bool power_save_on) override {}
  void SetLimit(float limit) override {}

 private:
  Delegate* delegate_;
  float volume_ = 0.0f;
  bool muted_ = false;
};

// static
std::unique_ptr<SystemVolumeControl> SystemVolumeControl::Create(
    Delegate* delegate) {
  return std::make_unique<DesktopSystemVolumeControl>(delegate);
}

}  // namespace media
}  // namespace chromecast
