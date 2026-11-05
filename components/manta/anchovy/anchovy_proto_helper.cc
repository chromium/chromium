// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_proto_helper.h"

#include <cmath>
#include <string_view>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/anchovy.pb.h"

namespace manta::anchovy {

namespace {
std::string_view AnchovyDescriptionTypeToString(
    proto::AnchovyDescription::Type type) {
  switch (type) {
    // Distinguish primary conceptual captions (GARCON_CONCEPTUAL) from
    // secondary noun-phrase appendices (GARCON_CNP) so the caller only uses
    // secondary captions when paired with a primary caption.
    case proto::AnchovyDescription_Type_PRIMARY_CAPTION:
      return kTypePrimaryCaption;
    case proto::AnchovyDescription_Type_SECONDARY_CAPTION:
      return kTypeSecondaryCaption;
    case proto::AnchovyDescription_Type_LABEL:
      return kTypeLabel;
    case proto::AnchovyDescription_Type_OCR:
      return kTypeOcr;
    case proto::AnchovyDescription_Type_UNKNOWN:
    case proto::AnchovyDescription_Type_UNUSED:
    case proto::
        AnchovyDescription_Type_AnchovyDescription_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        AnchovyDescription_Type_AnchovyDescription_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
      return "";
  }
  return "";
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
    std::move(callback).Run(base::DictValue(), std::move(manta_status));
    return;
  }

  CHECK(manta_response);

  base::ListValue results;
  for (const auto& data : manta_response->output_data()) {
    // Skip output entries without a valid serialized AnchovyDescription proto,
    // non-finite score, empty text, or an unrecognized description type.
    if (!data.has_custom() || !std::isfinite(data.score())) {
      continue;
    }
    proto::AnchovyDescription description;
    if (!description.ParseFromString(data.custom().value()) ||
        description.text().empty()) {
      continue;
    }
    std::string_view type = AnchovyDescriptionTypeToString(description.type());
    if (type.empty()) {
      continue;
    }
    results.Append(base::DictValue()
                       .Set("text", description.text())
                       .Set("type", type)
                       .Set("score", data.score()));
  }

  // An empty response (or one with no valid entries) is still an acceptable
  // response.
  if (results.empty()) {
    std::move(callback).Run(base::DictValue(), std::move(manta_status));
    return;
  }

  std::move(callback).Run(base::DictValue().Set("results", std::move(results)),
                          std::move(manta_status));
}

}  // namespace manta::anchovy
