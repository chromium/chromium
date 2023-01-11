// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_VIDEO_MODE_SWITCHER_H_
#define CHROMECAST_MEDIA_BASE_VIDEO_MODE_SWITCHER_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace media {
class VideoDecoderConfig;
}  // namespace media

namespace chromecast {
namespace media {

// Interface that implements video mode switching for CastRenderer.
class VideoModeSwitcher {
 public:
  virtual ~VideoModeSwitcher();

  // Input parameter is true if mode switch succeeded (or wasn't needed) and
  // playback should proceed. False indicates failed mode switch.
  using CompletionCB = base::OnceCallback<void(bool)>;

  // The |video_configs| describe input streams (potentially multiple streams in
  // case of dual layer content). The |mode_switch_completion_cb| will be
  // invoked to notify the caller about mode switch result.
  virtual void SwitchMode(
      const std::vector<::media::VideoDecoderConfig>& video_configs,
      CompletionCB mode_switch_completion_cb) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_VIDEO_MODE_SWITCHER_H_
