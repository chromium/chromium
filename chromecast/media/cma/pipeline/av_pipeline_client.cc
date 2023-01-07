// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/av_pipeline_client.h"

namespace chromecast {
namespace media {

AvPipelineClient::AvPipelineClient() {
}

AvPipelineClient::AvPipelineClient(AvPipelineClient&& other) = default;
AvPipelineClient& AvPipelineClient::operator=(AvPipelineClient&& other) =
    default;

AvPipelineClient::~AvPipelineClient() {
}

}  // namespace media
}  // namespace chromecast
