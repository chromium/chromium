// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/api/test/mock_cma_backend.h"

namespace chromecast {
namespace media {

MockCmaBackend::DecoderDelegate::DecoderDelegate() = default;
MockCmaBackend::DecoderDelegate::~DecoderDelegate() = default;

MockCmaBackend::AudioDecoder::AudioDecoder() = default;
MockCmaBackend::AudioDecoder::~AudioDecoder() = default;

MockCmaBackend::VideoDecoder::VideoDecoder() = default;
MockCmaBackend::VideoDecoder::~VideoDecoder() = default;

MockCmaBackend::MockCmaBackend() = default;
MockCmaBackend::~MockCmaBackend() = default;

}  // namespace media
}  // namespace chromecast
