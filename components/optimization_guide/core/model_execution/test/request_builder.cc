// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/request_builder.h"

#include "components/optimization_guide/proto/features/compose.pb.h"

namespace optimization_guide {

proto::features::ComposeRequest PageUrlRequest(const std::string& input) {
  proto::features::ComposeRequest req;
  req.mutable_page_metadata()->set_page_url(std::string(input));
  return req;
}

proto::features::ComposeRequest UserInputRequest(const std::string& input) {
  proto::features::ComposeRequest req;
  req.mutable_generate_params()->set_user_input(input);
  return req;
}

proto::features::ComposeRequest RewriteRequest(
    const std::string& previous_response) {
  proto::features::ComposeRequest req;
  auto& rewrite_params = *req.mutable_rewrite_params();
  rewrite_params.set_previous_response(previous_response);
  rewrite_params.set_tone(proto::features::COMPOSE_FORMAL);
  return req;
}

}  // namespace optimization_guide
