// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/chrome_media_drm_bridge_client.h"

ChromeMediaDrmBridgeClient::ChromeMediaDrmBridgeClient() {}

ChromeMediaDrmBridgeClient::~ChromeMediaDrmBridgeClient() {}

media::MediaDrmBridgeDelegate*
ChromeMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  if (scheme_uuid == widevine_delegate_.GetUUID())
    return &widevine_delegate_;
  if (scheme_uuid == clearkey_delegate_.GetUUID()) {
    return &clearkey_delegate_;
  }
  return nullptr;
}
