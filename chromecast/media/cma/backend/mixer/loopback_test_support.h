// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_TEST_SUPPORT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_TEST_SUPPORT_H_

#include <memory>

namespace chromecast {
namespace media {
class LoopbackHandler;

namespace mixer_service {
class MixerSocket;
}  // namespace mixer_service

std::unique_ptr<mixer_service::MixerSocket> CreateLoopbackConnectionForTest(
    LoopbackHandler* loopback_handler);
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_TEST_SUPPORT_H_
