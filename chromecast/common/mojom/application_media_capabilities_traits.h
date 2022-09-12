// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MOJOM_APPLICATION_MEDIA_CAPABILITIES_TRAITS_H_
#define CHROMECAST_COMMON_MOJOM_APPLICATION_MEDIA_CAPABILITIES_TRAITS_H_

#include "chromecast/base/bitstream_audio_codecs.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {
template <>
struct StructTraits<chromecast::shell::mojom::BitstreamAudioCodecsInfoDataView,
                    chromecast::BitstreamAudioCodecsInfo> {
  static int32_t codecs(const chromecast::BitstreamAudioCodecsInfo& info) {
    return info.codecs;
  }

  static int32_t spatial_rendering(
      const chromecast::BitstreamAudioCodecsInfo& info) {
    return info.spatial_rendering;
  }

  static bool Read(
      chromecast::shell::mojom::BitstreamAudioCodecsInfoDataView input,
      chromecast::BitstreamAudioCodecsInfo* output) {
    output->codecs = input.codecs();
    output->spatial_rendering = input.spatial_rendering();
    return true;
  }
};
}  // namespace mojo

#endif  // CHROMECAST_COMMON_MOJOM_APPLICATION_MEDIA_CAPABILITIES_TRAITS_H_
