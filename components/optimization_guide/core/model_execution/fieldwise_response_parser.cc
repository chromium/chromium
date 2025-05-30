// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/fieldwise_response_parser.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

namespace {

std::optional<ResponseParsingError> ExecuteExtractor(
    const proto::FieldExtractor& extractor,
    const std::string& output,
    google::protobuf::MessageLite& message) {
  if (!extractor.has_capturing_regex()) {
    return ResponseParsingError::kInvalidConfiguration;
  }

  const RE2 capturing_regex = extractor.capturing_regex();
  if (capturing_regex.NumberOfCapturingGroups() != 1) {
    return ResponseParsingError::kInvalidConfiguration;
  }

  std::string content;
  if (!RE2::PartialMatch(output, capturing_regex, &content)) {
    // Do nothing if the regex doesn't match.
    return std::nullopt;
  }

  const auto loc = extractor.translation_map().find(content);
  if (loc != extractor.translation_map().end()) {
    content = loc->second;
  }

  const ProtoStatus status =
      SetProtoValueFromString(&message, extractor.output_field(), content);

  if (status != ProtoStatus::kOk) {
    return ResponseParsingError::kInvalidConfiguration;
  }

  return std::nullopt;
}

}  // namespace

FieldwiseResponseParser::FieldwiseResponseParser(
    std::string_view proto_type,
    proto::FieldwiseParserConfig& config,
    bool suppress_parsing_incomplete_response)
    : proto_type_(proto_type),
      config_(config),
      suppress_parsing_incomplete_response_(
          suppress_parsing_incomplete_response) {}

void FieldwiseResponseParser::ParseAsync(const std::string& redacted_output,
                                         ResultCallback result_callback) const {
  const std::unique_ptr<google::protobuf::MessageLite> message =
      BuildMessage(proto_type_);

  if (!message) {
    std::move(result_callback)
        .Run(base::unexpected(ResponseParsingError::kInvalidConfiguration));
    return;
  }

  for (const proto::FieldExtractor& extractor : config_.field_extractors()) {
    std::optional<ResponseParsingError> error =
        ExecuteExtractor(extractor, redacted_output, *message);

    if (error) {
      std::move(result_callback).Run(base::unexpected(*error));
      return;
    }
  }

  std::move(result_callback).Run(AnyWrapProto(*message));
}

bool FieldwiseResponseParser::SuppressParsingIncompleteResponse() const {
  return suppress_parsing_incomplete_response_;
}

}  // namespace optimization_guide
