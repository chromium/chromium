// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
#define CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_

#include <vector>

#include "base/macros.h"
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
  ~MediaCapsImpl() override;

  void Initialize();
  void AddReceiver(mojo::PendingReceiver<mojom::MediaCaps> receiver);

  void ScreenResolutionChanged(unsigned width, unsigned height);
  void ScreenInfoChanged(int hdcp_version,
                         int supported_eotfs,
                         int dolby_vision_flags,
                         int screen_width_mm,
                         int screen_height_mm,
                         bool current_mode_supports_hdr,
                         bool current_mode_supports_dv);
  void AddSupportedCodecProfileLevel(
      const CodecProfileLevel& codec_profile_level);

 private:
  // chromecast::mojom::MediaCaps implementation.
  void AddObserver(
      mojo::PendingRemote<mojom::MediaCapsObserver> observer) override;

  int hdcp_version_;
  int supported_eotfs_;
  int dolby_vision_flags_;
  int screen_width_mm_;
  int screen_height_mm_;
  bool current_mode_supports_hdr_;
  bool current_mode_supports_dv_;
  gfx::Size screen_resolution_;
  std::vector<CodecProfileLevel> codec_profile_levels_;
  mojo::RemoteSet<mojom::MediaCapsObserver> observers_;
  mojo::ReceiverSet<mojom::MediaCaps> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapsImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
