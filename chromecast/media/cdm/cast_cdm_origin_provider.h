// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_ORIGIN_PROVIDER_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_ORIGIN_PROVIDER_H_

namespace media {
namespace mojom {
class FrameInterfaceFactory;
}  // namespace mojom
}  // namespace media

namespace url {
class Origin;
}  // namespace url

namespace chromecast {

class CastCdmOriginProvider {
 public:
  // Util function to call sync mojo API to get cdm origin.
  // TODO(159346933) Remove once the origin isolation logic is moved outside of
  // media service.
  static bool GetCdmOrigin(::media::mojom::FrameInterfaceFactory* interfaces,
                           url::Origin* cdm_origin);
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_ORIGIN_PROVIDER_H_
