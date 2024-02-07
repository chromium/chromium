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

namespace chromecast {
namespace media {

struct CodecProfileLevel;

class MediaCapsImpl : public mojom::MediaCaps {
 public:
  MediaCapsImpl();

  MediaCapsImpl(const MediaCapsImpl&) = delete;
  MediaCapsImpl& operator=(const MediaCapsImpl&) = delete;

  ~MediaCapsImpl() override;

  void AddReceiver(mojo::PendingReceiver<mojom::MediaCaps> receiver);
  mojo::PendingRemote<mojom::MediaCaps> GetPendingRemote();
  void AddSupportedCodecProfileLevel(
      const CodecProfileLevel& codec_profile_level);

 private:
  // chromecast::mojom::MediaCaps implementation.
  void AddObserver(
      mojo::PendingRemote<mojom::MediaCapsObserver> observer) override;

  std::vector<CodecProfileLevel> codec_profile_levels_;
  mojo::RemoteSet<mojom::MediaCapsObserver> observers_;
  mojo::ReceiverSet<mojom::MediaCaps> receivers_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
