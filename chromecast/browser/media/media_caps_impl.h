// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
#define CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_

#include <vector>

#include "chromecast/common/mojom/media_caps.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

struct CodecProfileLevel;

class MediaCapsImpl : public mojom::MediaCaps {
 public:
  MediaCapsImpl();

  MediaCapsImpl(const MediaCapsImpl&) = delete;
  MediaCapsImpl& operator=(const MediaCapsImpl&) = delete;

  ~MediaCapsImpl() override;

  void Initialize();
  void AddReceiver(mojo::PendingReceiver<mojom::MediaCaps> receiver);
  mojo::PendingRemote<mojom::MediaCaps> GetPendingRemote();
  void AddSupportedCodecProfileLevel(
      const CodecProfileLevel& codec_profile_level);

 private:
  // chromecast::mojom::MediaCaps implementation.
  void AddObserver(
      mojo::PendingRemote<mojom::MediaCapsObserver> observer) override;
  void ScreenResolutionChanged(uint32_t width, uint32_t height) override;
  void ScreenInfoChanged(int32_t hdcp_version,
                         int32_t supported_eotfs,
                         int32_t dolby_vision_flags,
                         int32_t screen_width_mm,
                         int32_t screen_height_mm,
                         bool current_mode_supports_hdr,
                         bool current_mode_supports_dv) override;

  int32_t hdcp_version_;
  int32_t supported_eotfs_;
  int32_t dolby_vision_flags_;
  int32_t screen_width_mm_;
  int32_t screen_height_mm_;
  bool current_mode_supports_hdr_;
  bool current_mode_supports_dv_;
  gfx::Size screen_resolution_;
  std::vector<CodecProfileLevel> codec_profile_levels_;
  mojo::RemoteSet<mojom::MediaCapsObserver> observers_;
  mojo::ReceiverSet<mojom::MediaCaps> receivers_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
