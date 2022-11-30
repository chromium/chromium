// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_PLAYREADY_DRM_DELEGATE_ANDROID_H_
#define CHROMECAST_MEDIA_CDM_PLAYREADY_DRM_DELEGATE_ANDROID_H_

#include <stdint.h>

#include "media/base/android/media_drm_bridge_delegate.h"

namespace chromecast {
namespace media {

class PlayreadyDrmDelegateAndroid : public ::media::MediaDrmBridgeDelegate {
 public:
  PlayreadyDrmDelegateAndroid();

  PlayreadyDrmDelegateAndroid(const PlayreadyDrmDelegateAndroid&) = delete;
  PlayreadyDrmDelegateAndroid& operator=(const PlayreadyDrmDelegateAndroid&) =
      delete;

  ~PlayreadyDrmDelegateAndroid() override;

  // ::media::MediaDrmBridgeDelegate implementation:
  const ::media::UUID GetUUID() const override;
  bool OnCreateSession(
      const ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::vector<uint8_t>* init_data_out,
      std::vector<std::string>* optional_parameters_out) override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_PLAYREADY_DRM_DELEGATE_ANDROID_H_
