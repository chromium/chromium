// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_proto_helper.h"

#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
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

constexpr char kAnchovyDescriptionsContextTypeUrl[] =
    "type.googleapis.com/image_repository.api.ImageDescriptionContext";

// Initializes the shared feature name and language context for an Anchovy
// image description request.
proto::Request CreateBaseAnchovyRequest(std::string_view lang_tag) {
  proto::Request request_proto;
  request_proto.set_feature_name(
      proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);

  proto::AnchovyContext context;
  context.set_lang_id(lang_tag);
  context.mutable_optional_inputs()->set_input1(true);

  // Add model-specific context options shared across all images in the request.
  auto* specific_options =
      request_proto.mutable_request_config()->mutable_specific_options();
  specific_options->set_type_url(kAnchovyDescriptionsContextTypeUrl);
  specific_options->set_value(context.SerializeAsString());
  return request_proto;
}

// Validates and converts a single server `OutputData` entry into the parsed
// annotation dictionary format (`{"text", "type", "score"}`), returning
// `std::nullopt` when the entry is missing required fields or has an
// unrecognized description type.
std::optional<base::DictValue> ParseAnchovyOutputEntry(
    const proto::OutputData& data) {
  if (!data.has_custom() || !std::isfinite(data.score())) {
    return std::nullopt;
  }

  // Parse `custom.value` directly without inspecting `custom.type_url`, since
  // the server sets its internal protobuf type name rather than the client
  // mirror name.
  proto::AnchovyDescription description;
  if (!description.ParseFromString(data.custom().value()) ||
      description.text().empty()) {
    return std::nullopt;
  }

  std::string_view type = AnchovyDescriptionTypeToString(description.type());
  if (type.empty()) {
    return std::nullopt;
  }

  return base::DictValue()
      .Set("text", description.text())
      .Set("type", type)
      .Set("score", data.score());
}

}  // namespace

proto::Request AnchovyProtoHelper::ComposeRequest(
    const ImageDescriptionBatchRequest& request) {
  // Callers guarantee at least one image in every batch request.
  CHECK(!request.images.empty());

  proto::Request request_proto = CreateBaseAnchovyRequest(request.lang_tag);

  std::set<std::string_view> seen_tags;
  for (const auto& entry : request.images) {
    // Callers guarantee non-empty image payloads and unique, non-empty UTF-8
    // correlation tags.
    CHECK(!entry.tag.empty());
    CHECK(!entry.image_bytes->empty());
    CHECK(base::IsStringUTF8AllowingNoncharacters(entry.tag));
    CHECK(seen_tags.insert(entry.tag).second);

    auto* input_data = request_proto.add_input_data();
    input_data->set_tag(entry.tag);
    input_data->mutable_image()->set_serialized_bytes(
        entry.image_bytes->data(), entry.image_bytes->size());
  }

  return request_proto;
}

void AnchovyProtoHelper::HandleImageDescriptionsResponse(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    CHECK(manta_response == nullptr);
    std::move(callback).Run(base::DictValue(), std::move(manta_status));
    return;
  }

  CHECK(manta_response);

  base::DictValue results_by_tag;
  for (const auto& data : manta_response->output_data()) {
    // Batched responses require a valid non-empty UTF-8 tag to attribute each
    // annotation back to its requested image (`EnsureList` requires UTF-8).
    if (data.tag().empty() ||
        !base::IsStringUTF8AllowingNoncharacters(data.tag())) {
      continue;
    }
    if (std::optional<base::DictValue> entry = ParseAnchovyOutputEntry(data)) {
      results_by_tag.EnsureList(data.tag())->Append(std::move(*entry));
    }
  }

  // Always include `kResultsByTagKey` on `kOk` (even if empty when all
  // candidates were filtered out) so callers receive a uniform dictionary
  // schema.
  std::move(callback).Run(
      base::DictValue().Set(kResultsByTagKey, std::move(results_by_tag)),
      std::move(manta_status));
}

}  // namespace manta::anchovy
