// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_

#include "chromecast/common/mojom/media_caps.mojom.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromecast {
namespace media {

class MediaCapsObserverImpl : public mojom::MediaCapsObserver {
 public:
  MediaCapsObserverImpl(mojo::PendingRemote<mojom::MediaCapsObserver>* proxy,
                        SupportedCodecProfileLevelsMemo* supported_profiles);

  MediaCapsObserverImpl(const MediaCapsObserverImpl&) = delete;
  MediaCapsObserverImpl& operator=(const MediaCapsObserverImpl&) = delete;

  ~MediaCapsObserverImpl() override;

 private:
  void AddSupportedCodecProfileLevel(
      mojom::CodecProfileLevelPtr codec_profile_level) override;

  SupportedCodecProfileLevelsMemo* supported_profiles_;
  mojo::Receiver<mojom::MediaCapsObserver> receiver_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_
