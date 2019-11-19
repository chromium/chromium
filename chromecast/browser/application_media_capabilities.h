// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_
#define CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_

#include "base/macros.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {
namespace shell {

class ApplicationMediaCapabilities
    : public mojom::ApplicationMediaCapabilities {
 public:
  ApplicationMediaCapabilities();
  ~ApplicationMediaCapabilities() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::ApplicationMediaCapabilities> receiver);

  void SetSupportedBitstreamAudioCodecs(int codecs);

 private:
  // mojom::ApplicationMediaCapabilities implementation:
  void AddObserver(
      mojo::PendingRemote<mojom::ApplicationMediaCapabilitiesObserver>
          observer_remote) override;

  mojo::ReceiverSet<mojom::ApplicationMediaCapabilities> receivers_;
  mojo::RemoteSet<mojom::ApplicationMediaCapabilitiesObserver> observers_;
  int supported_bitstream_audio_codecs_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationMediaCapabilities);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_
