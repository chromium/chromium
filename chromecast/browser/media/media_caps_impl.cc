// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_caps_impl.h"

#include "base/logging.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/public/media/decoder_config.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace media {

mojom::CodecProfileLevelPtr ConvertCodecProfileLevelToMojo(
    const CodecProfileLevel& codec_profile_level) {
  mojom::CodecProfileLevelPtr result = mojom::CodecProfileLevel::New();
  result->codec = codec_profile_level.codec;
  result->profile = codec_profile_level.profile;
  result->level = codec_profile_level.level;
  return result;
}

MediaCapsImpl::MediaCapsImpl()
    : hdcp_version_(0),
      supported_eotfs_(0),
      dolby_vision_flags_(0),
      screen_width_mm_(0),
      screen_height_mm_(0),
      current_mode_supports_hdr_(false),
      current_mode_supports_dv_(false),
      screen_resolution_(0, 0) {}

MediaCapsImpl::~MediaCapsImpl() = default;

void MediaCapsImpl::AddReceiver(
    mojo::PendingReceiver<mojom::MediaCaps> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojo::PendingRemote<mojom::MediaCaps> MediaCapsImpl::GetPendingRemote() {
  mojo::PendingRemote<mojom::MediaCaps> pending_remote;
  AddReceiver(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void MediaCapsImpl::ScreenResolutionChanged(uint32_t width, uint32_t height) {
  screen_resolution_ = gfx::Size(width, height);
  for (auto& observer : observers_)
    observer->ScreenResolutionChanged(width, height);
}

void MediaCapsImpl::ScreenInfoChanged(int32_t hdcp_version,
                                      int32_t supported_eotfs,
                                      int32_t dolby_vision_flags,
                                      int32_t screen_width_mm,
                                      int32_t screen_height_mm,
                                      bool current_mode_supports_hdr,
                                      bool current_mode_supports_dv) {
  hdcp_version_ = hdcp_version;
  supported_eotfs_ = supported_eotfs;
  dolby_vision_flags_ = dolby_vision_flags;
  screen_width_mm_ = screen_width_mm;
  screen_height_mm_ = screen_height_mm;
  current_mode_supports_hdr_ = current_mode_supports_hdr;
  current_mode_supports_dv_ = current_mode_supports_dv;

  for (auto& observer : observers_) {
    observer->ScreenInfoChanged(hdcp_version, supported_eotfs,
                                dolby_vision_flags, screen_width_mm,
                                screen_height_mm, current_mode_supports_hdr,
                                current_mode_supports_dv);
  }
}

void MediaCapsImpl::AddSupportedCodecProfileLevel(
    const CodecProfileLevel& codec_profile_level) {
  codec_profile_levels_.push_back(codec_profile_level);
  for (auto& observer : observers_) {
    mojom::CodecProfileLevelPtr mojo_codec_profile_level(
        ConvertCodecProfileLevelToMojo(codec_profile_level));
    observer->AddSupportedCodecProfileLevel(
        std::move(mojo_codec_profile_level));
  }
}

void MediaCapsImpl::AddObserver(
    mojo::PendingRemote<mojom::MediaCapsObserver> observer_remote) {
  mojo::Remote<mojom::MediaCapsObserver> observer(std::move(observer_remote));
  observer->ScreenResolutionChanged(screen_resolution_.width(),
                                    screen_resolution_.height());
  observer->ScreenInfoChanged(hdcp_version_, supported_eotfs_,
                              dolby_vision_flags_, screen_width_mm_,
                              screen_height_mm_, current_mode_supports_hdr_,
                              current_mode_supports_dv_);
  DVLOG(1) << __func__ << ": Sending " << codec_profile_levels_.size()
           << " supported codec profile levels to observer.";
  for (const auto& codec_profile_level : codec_profile_levels_) {
    observer->AddSupportedCodecProfileLevel(
        ConvertCodecProfileLevelToMojo(codec_profile_level));
  }
  observers_.Add(std::move(observer));
}

}  // namespace media
}  // namespace chromecast
