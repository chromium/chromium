// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_proto_helper.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/anchovy.pb.h"

namespace manta::anchovy {

namespace {
std::string AnchovyDescriptionTypeToString(
    proto::AnchovyDescription::Type type) {
  switch (type) {
    case proto::AnchovyDescription_Type_PRIMARY_CAPTION:
    case proto::AnchovyDescription_Type_SECONDARY_CAPTION:
      return "CAPTION";
    case proto::AnchovyDescription_Type_LABEL:
      return "LABEL";
    case proto::AnchovyDescription_Type_OCR:
      return "OCR";
    case proto::AnchovyDescription_Type_UNKNOWN:
    case proto::AnchovyDescription_Type_UNUSED:
    case proto::
        AnchovyDescription_Type_AnchovyDescription_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        AnchovyDescription_Type_AnchovyDescription_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
      return "";
  }
}
}  // namespace

constexpr char kAnchovyDescriptionsContextTypeUrl[] =
    "type.googleapis.com/image_repository.api.ImageDescriptionContext";

proto::Request AnchovyProtoHelper::ComposeRequest(
    const ImageDescriptionRequest& request) {
  proto::Request request_proto;
  request_proto.set_feature_name(
      proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);

  proto::AnchovyContext context;
  context.set_lang_id(request.lang_tag);
  context.mutable_optional_inputs()->set_input1(true);

  // Add model specific options to the request.
  request_proto.mutable_request_config()
      ->mutable_specific_options()
      ->set_type_url(kAnchovyDescriptionsContextTypeUrl);
  request_proto.mutable_request_config()->mutable_specific_options()->set_value(
      context.SerializeAsString());

  auto* input_data = request_proto.add_input_data();
  input_data->mutable_image()->set_serialized_bytes(
      std::string(request.image_bytes->begin(), request.image_bytes->end()));

  return request_proto;
}

void AnchovyProtoHelper::HandleImageDescriptionResponse(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    CHECK(manta_response == nullptr);
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  CHECK(manta_response);

  // An empty response is still an acceptable response.
  if (manta_response->output_data_size() < 1 ||
      !manta_response->output_data(0).has_custom()) {
    std::move(callback).Run(base::Value::Dict(),
                            {MantaStatusCode::kOk, std::string()});
    return;
  }

  base::Value::List results;
  for (const auto& data : manta_response->output_data()) {
    if (data.has_custom()) {
      proto::AnchovyDescription description;
      description.ParseFromString(data.custom().value());
      results.Append(
          base::Value::Dict()
              .Set("text", description.text())
              .Set("type", AnchovyDescriptionTypeToString(description.type()))
              .Set("score", data.score()));
    }
  }

  std::move(callback).Run(
      base::Value::Dict().Set("results", std::move(results)),
      std::move(manta_status));
}

}  // namespace manta::anchovy
