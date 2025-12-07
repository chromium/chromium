// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/simple_response_parser.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"

namespace optimization_guide {

SimpleResponseParser::SimpleResponseParser(
    std::string_view proto_type,
    const proto::ProtoField& proto_field,
    bool suppress_parsing_incomplete_response)
    : proto_type_(proto_type),
      proto_field_(proto_field),
      suppress_parsing_incomplete_response_(
          suppress_parsing_incomplete_response) {}

void SimpleResponseParser::ParseAsync(const std::string& redacted_output,
                                      ResultCallback result_callback) const {
  std::unique_ptr<google::protobuf::MessageLite> message =
      BuildMessage(proto_type_);

  if (!message) {
    std::move(result_callback)
        .Run(base::unexpected(ResponseParsingError::kInvalidConfiguration));
    return;
  }

  ProtoStatus status =
      SetProtoValueFromString(message.get(), proto_field_, redacted_output);

  if (status != ProtoStatus::kOk) {
    std::move(result_callback)
        .Run(base::unexpected(ResponseParsingError::kInvalidConfiguration));
    return;
  }

  std::move(result_callback).Run(AnyWrapProto(*message));
}

bool SimpleResponseParser::SuppressParsingIncompleteResponse() const {
  return suppress_parsing_incomplete_response_;
}

}  // namespace optimization_guide
