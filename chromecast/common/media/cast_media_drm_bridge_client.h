// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MEDIA_CAST_MEDIA_DRM_BRIDGE_CLIENT_H_
#define CHROMECAST_COMMON_MEDIA_CAST_MEDIA_DRM_BRIDGE_CLIENT_H_

#include "chromecast/chromecast_buildflags.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"

namespace chromecast {
namespace media {

class CastMediaDrmBridgeClient : public ::media::MediaDrmBridgeClient {
 public:
  CastMediaDrmBridgeClient();

  CastMediaDrmBridgeClient(const CastMediaDrmBridgeClient&) = delete;
  CastMediaDrmBridgeClient& operator=(const CastMediaDrmBridgeClient&) = delete;

  ~CastMediaDrmBridgeClient() override;

 private:
  // ::media::MediaDrmBridgeClient implementation:
  void AddKeySystemUUIDMappings(KeySystemUuidMap* map) override;
  ::media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const ::media::UUID& scheme_uuid) override;

  cdm::WidevineDrmDelegateAndroid widevine_delegate_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_MEDIA_CAST_MEDIA_DRM_BRIDGE_CLIENT_H_
