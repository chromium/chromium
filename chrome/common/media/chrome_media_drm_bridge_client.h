// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CHROME_MEDIA_DRM_BRIDGE_CLIENT_H_
#define CHROME_COMMON_MEDIA_CHROME_MEDIA_DRM_BRIDGE_CLIENT_H_

#include <stdint.h>

#include "components/cdm/common/clearkey_drm_delegate_android.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"

class ChromeMediaDrmBridgeClient : public media::MediaDrmBridgeClient {
 public:
  ChromeMediaDrmBridgeClient();

  ChromeMediaDrmBridgeClient(const ChromeMediaDrmBridgeClient&) = delete;
  ChromeMediaDrmBridgeClient& operator=(const ChromeMediaDrmBridgeClient&) =
      delete;

  ~ChromeMediaDrmBridgeClient() override;

 private:
  // media::MediaDrmBridgeClient implementation:
  media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const std::vector<uint8_t>& scheme_uuid) override;

  cdm::WidevineDrmDelegateAndroid widevine_delegate_;

  cdm::ClearKeyDrmDelegateAndroid clearkey_delegate_;
};

#endif  // CHROME_COMMON_MEDIA_CHROME_MEDIA_DRM_BRIDGE_CLIENT_H_
