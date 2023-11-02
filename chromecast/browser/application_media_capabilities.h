// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_
#define CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_

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

  ApplicationMediaCapabilities(const ApplicationMediaCapabilities&) = delete;
  ApplicationMediaCapabilities& operator=(const ApplicationMediaCapabilities&) =
      delete;

  ~ApplicationMediaCapabilities() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::ApplicationMediaCapabilities> receiver);

  void SetSupportedBitstreamAudioCodecs(const BitstreamAudioCodecsInfo& info);

 private:
  // mojom::ApplicationMediaCapabilities implementation:
  void AddObserver(
      mojo::PendingRemote<mojom::ApplicationMediaCapabilitiesObserver>
          observer_remote) override;

  mojo::ReceiverSet<mojom::ApplicationMediaCapabilities> receivers_;
  mojo::RemoteSet<mojom::ApplicationMediaCapabilitiesObserver> observers_;
  BitstreamAudioCodecsInfo supported_bitstream_audio_codecs_info_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_MEDIA_CAPABILITIES_H_
