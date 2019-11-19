// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/media_caps_observer_impl.h"

#include "chromecast/media/base/media_caps.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

MediaCapsObserverImpl::MediaCapsObserverImpl(
    mojo::PendingRemote<mojom::MediaCapsObserver>* proxy,
    SupportedCodecProfileLevelsMemo* supported_profiles)
    : supported_profiles_(supported_profiles),
      receiver_(this, proxy->InitWithNewPipeAndPassReceiver()) {}

MediaCapsObserverImpl::~MediaCapsObserverImpl() = default;

void MediaCapsObserverImpl::ScreenResolutionChanged(uint32_t width,
                                                    uint32_t height) {
  MediaCapabilities::ScreenResolutionChanged(gfx::Size(width, height));
}

void MediaCapsObserverImpl::ScreenInfoChanged(int32_t hdcp_version,
                                              int32_t supported_eotfs,
                                              int32_t dolby_vision_flags,
                                              int32_t screen_width_mm,
                                              int32_t screen_height_mm,
                                              bool current_mode_supports_hdr,
                                              bool current_mode_supports_dv) {
  MediaCapabilities::ScreenInfoChanged(
      hdcp_version, supported_eotfs, dolby_vision_flags, screen_width_mm,
      screen_height_mm, current_mode_supports_hdr, current_mode_supports_dv);
}

void MediaCapsObserverImpl::AddSupportedCodecProfileLevel(
    mojom::CodecProfileLevelPtr codec_profile_level) {
  CodecProfileLevel converted_codec_profile_level(
      {static_cast<VideoCodec>(codec_profile_level->codec),
       static_cast<VideoProfile>(codec_profile_level->profile),
       codec_profile_level->level});
  supported_profiles_->AddSupportedCodecProfileLevel(
      converted_codec_profile_level);
}

}  // namespace media
}  // namespace chromecast
