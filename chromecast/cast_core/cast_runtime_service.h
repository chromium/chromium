// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_
#define CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_

#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_endpoint_manager.h"
#include "chromecast/service/cast_service.h"

namespace chromecast {

class WebCryptoServer;

namespace receiver {
class MediaManager;
}  // namespace receiver

// This interface is to be used for building the Cast Runtime Service and act as
// the border between shared Chromium code and the specifics of that
// implementation.
//
// NOTE: When adding a new interface to this class, first add it to all
// implementations of this interface in downstream repos. Else, the roll of this
// code into those repos will break.
class CastRuntimeService
    : public CastService,
      public media::CastRuntimeAudioChannelEndpointManager {
 public:
  // Returns current instance of CastRuntimeService in the browser process.
  static CastRuntimeService* GetInstance();

  CastRuntimeService();
  ~CastRuntimeService() override;

  virtual WebCryptoServer* GetWebCryptoServer() = 0;
  virtual receiver::MediaManager* GetMediaManager() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_
