// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_

#include "components/optimization_guide/proto/features/compose.pb.h"
#include "services/on_device_model/ml/chrome_ml_audio_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace optimization_guide {

// Creates a width x height black image.
SkBitmap CreateBlackSkBitmap(int width, int height);

// Create a trivial ml::AudioBuffer.
ml::AudioBuffer CreateDummyAudioBuffer();

// A ComposeRequest with page_metadata.page_url filled.
proto::ComposeRequest PageUrlRequest(const std::string& input);

// A ComposeRequest with generate_params.user_input filled.
proto::ComposeRequest UserInputRequest(const std::string& input);

// A ComposeRequest with rewrite_params.previous_response filled.
proto::ComposeRequest RewriteRequest(const std::string& previous_response);

proto::Any ComposeResponse(const std::string& output);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_
