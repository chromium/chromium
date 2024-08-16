// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_proto_helper.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/manta/manta_service_callbacks.h"

namespace manta::anchovy {

proto::Request AnchovyProtoHelper::ComposeRequest(
    const ImageDescriptionRequest& request) {
  proto::Request request_proto;
  request_proto.set_feature_name(
      proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);

  auto* input_data = request_proto.add_input_data();
  input_data->mutable_image()->set_serialized_bytes(
      std::string(request.image_bytes->begin(), request.image_bytes->end()));

  return request_proto;
}

void AnchovyProtoHelper::HandleImageDescriptionResponse(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  LOG(ERROR) << "Using Public Handler.";
  std::move(callback).Run(base::Value::Dict(),
                          {MantaStatusCode::kGenericError, std::string()});
  return;
}

}  // namespace manta::anchovy
