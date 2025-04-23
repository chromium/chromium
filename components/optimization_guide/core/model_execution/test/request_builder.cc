// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/request_builder.h"

#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"

namespace optimization_guide {

SkBitmap CreateBlackSkBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLACK);
  return bitmap;
}

proto::ComposeRequest PageUrlRequest(const std::string& input) {
  proto::ComposeRequest req;
  req.mutable_page_metadata()->set_page_url(std::string(input));
  return req;
}

proto::ComposeRequest UserInputRequest(const std::string& input) {
  proto::ComposeRequest req;
  req.mutable_generate_params()->set_user_input(input);
  return req;
}

proto::ComposeRequest RewriteRequest(const std::string& previous_response) {
  proto::ComposeRequest req;
  auto& rewrite_params = *req.mutable_rewrite_params();
  rewrite_params.set_previous_response(previous_response);
  rewrite_params.set_tone(proto::COMPOSE_FORMAL);
  return req;
}

proto::Any ComposeResponse(const std::string& output) {
  proto::ComposeResponse response;
  response.set_output(output);
  return AnyWrapProto(response);
}

}  // namespace optimization_guide
