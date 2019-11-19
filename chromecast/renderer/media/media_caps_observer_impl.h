// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/common/mojom/media_caps.mojom.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

class MediaCapsObserverImpl : public mojom::MediaCapsObserver {
 public:
  MediaCapsObserverImpl(mojo::PendingRemote<mojom::MediaCapsObserver>* proxy,
                        SupportedCodecProfileLevelsMemo* supported_profiles);
  ~MediaCapsObserverImpl() override;

 private:
  void ScreenResolutionChanged(uint32_t width, uint32_t height) override;
  void ScreenInfoChanged(int32_t hdcp_version,
                         int32_t supported_eotfs,
                         int32_t dolby_vision_flags,
                         int32_t screen_width_mm,
                         int32_t screen_height_mm,
                         bool current_mode_supports_hdr,
                         bool current_mode_supports_dolby_vision) override;
  void AddSupportedCodecProfileLevel(
      mojom::CodecProfileLevelPtr codec_profile_level) override;

  SupportedCodecProfileLevelsMemo* supported_profiles_;
  mojo::Receiver<mojom::MediaCapsObserver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapsObserverImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_
