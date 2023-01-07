// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_client.h"

namespace chromecast {
namespace media {

MediaPipelineClient::MediaPipelineClient() {
}

MediaPipelineClient::MediaPipelineClient(MediaPipelineClient&& other) = default;

MediaPipelineClient& MediaPipelineClient::operator=(
    MediaPipelineClient&& other) = default;

MediaPipelineClient::~MediaPipelineClient() {
}

}  // namespace media
}  // namespace chromecast
