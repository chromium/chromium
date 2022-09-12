// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/video_pipeline_client.h"

namespace chromecast {
namespace media {

VideoPipelineClient::VideoPipelineClient() {
}

VideoPipelineClient::VideoPipelineClient(VideoPipelineClient&& other) = default;
VideoPipelineClient& VideoPipelineClient::operator=(
    VideoPipelineClient&& other) = default;

VideoPipelineClient::~VideoPipelineClient() {
}

}  // namespace media
}  // namespace chromecast
