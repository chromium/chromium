// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_

#include "components/optimization_guide/proto/features/compose.pb.h"

namespace optimization_guide {

// A ComposeRequest with page_metadata.page_url filled.
proto::ComposeRequest PageUrlRequest(const std::string& input);

// A ComposeRequest with generate_params.user_input filled.
proto::ComposeRequest UserInputRequest(const std::string& input);

// A ComposeRequest with rewrite_params.previous_response filled.
proto::ComposeRequest RewriteRequest(const std::string& previous_response);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_REQUEST_BUILDER_H_
