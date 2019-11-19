// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_capabilities.h"

#include <utility>

#include "chromecast/base/bitstream_audio_codecs.h"

namespace chromecast {
namespace shell {

ApplicationMediaCapabilities::ApplicationMediaCapabilities()
    : supported_bitstream_audio_codecs_(kBitstreamAudioCodecNone) {}

ApplicationMediaCapabilities::~ApplicationMediaCapabilities() = default;

void ApplicationMediaCapabilities::AddReceiver(
    mojo::PendingReceiver<mojom::ApplicationMediaCapabilities> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ApplicationMediaCapabilities::SetSupportedBitstreamAudioCodecs(
    int codecs) {
  supported_bitstream_audio_codecs_ = codecs;
  for (auto& observer : observers_)
    observer->OnSupportedBitstreamAudioCodecsChanged(codecs);
}

void ApplicationMediaCapabilities::AddObserver(
    mojo::PendingRemote<mojom::ApplicationMediaCapabilitiesObserver>
        observer_remote) {
  mojo::Remote<mojom::ApplicationMediaCapabilitiesObserver> observer(
      std::move(observer_remote));
  observer->OnSupportedBitstreamAudioCodecsChanged(
      supported_bitstream_audio_codecs_);
  observers_.Add(std::move(observer));
}

}  // namespace shell
}  // namespace chromecast
