// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MOJOM_MULTIROOM_MOJOM_TRAITS_H_
#define CHROMECAST_COMMON_MOJOM_MULTIROOM_MOJOM_TRAITS_H_

#include "base/logging.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

template <>
struct mojo::EnumTraits<chromecast::mojom::AudioChannel,
                        chromecast::media::AudioChannel> {
  static chromecast::mojom::AudioChannel ToMojom(
      chromecast::media::AudioChannel input) {
    switch (input) {
      case chromecast::media::AudioChannel::kAll:
        return chromecast::mojom::AudioChannel::kAll;
      case chromecast::media::AudioChannel::kLeft:
        return chromecast::mojom::AudioChannel::kLeft;
      case chromecast::media::AudioChannel::kRight:
        return chromecast::mojom::AudioChannel::kRight;
    }
    NOTREACHED();
    return chromecast::mojom::AudioChannel::kAll;
  }

  static bool FromMojom(chromecast::mojom::AudioChannel input,
                        chromecast::media::AudioChannel* output) {
    DCHECK(output);
    switch (input) {
      case chromecast::mojom::AudioChannel::kAll:
        *output = chromecast::media::AudioChannel::kAll;
        return true;
      case chromecast::mojom::AudioChannel::kLeft:
        *output = chromecast::media::AudioChannel::kLeft;
        return true;
      case chromecast::mojom::AudioChannel::kRight:
        *output = chromecast::media::AudioChannel::kRight;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

#endif  // CHROMECAST_COMMON_MOJOM_MULTIROOM_MOJOM_TRAITS_H_
