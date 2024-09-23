// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "components/media_router/common/providers/cast/channel/fuzz_proto/fuzzer_inputs.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

using cast_util::EnumToString;

namespace cast_channel {
namespace fuzz {

namespace {

base::Value MakeValue(const JunkValue::Field& field) {
  if (field.has_int_value()) {
    return base::Value(field.int_value());
  }
  if (field.has_string_value()) {
    return base::Value(field.string_value());
  }
  if (field.has_float_value()) {
    return std::isfinite(field.float_value()) ? base::Value(field.float_value())
                                              : base::Value();
  }
  if (field.has_bool_value()) {
    return base::Value(field.bool_value());
  }
  return base::Value();
}

base::Value::Dict MakeDict(const JunkValue& junk) {
  base::Value::Dict result;
  for (const auto& field : junk.field()) {
    result.Set(field.name(), MakeValue(field));
  }
  return result;
}

base::Value MakeValue(const JunkValue& junk) {
  return base::Value(MakeDict(junk));
}

template <typename Field, typename T = typename Field::value_type>
std::vector<T> MakeVector(const Field& field) {
  return std::vector<T>(field.cbegin(), field.cend());
}

}  // namespace

DEFINE_PROTO_FUZZER(const CastMessageUtilInputs& input_union) {
  // TODO(crbug.com/40555657): Add test for CreateAuthChallengeMessage()
  switch (input_union.input_case()) {
    case CastMessageUtilInputs::kCreateLaunchRequestInput: {
      const auto& input = input_union.create_launch_request_input();
      std::optional<base::Value> app_params;
      if (input.has_app_params())
        app_params = MakeValue(input.app_params());
      CreateLaunchRequest(input.source_id(), input.request_id(), input.app_id(),
                          input.locale(),
                          MakeVector(input.supported_app_types()), app_params);
      break;
    }
    case CastMessageUtilInputs::kCreateStopRequestInput: {
      const auto& input = input_union.create_stop_request_input();
      CreateStopRequest(input.source_id(), input.request_id(),
                        input.session_id());
      break;
    }
    case CastMessageUtilInputs::kCreateCastMessageInput: {
      const auto& input = input_union.create_cast_message_input();
      base::Value body = MakeValue(input.body());
      CreateCastMessage(input.message_namespace(), body, input.source_id(),
                        input.destination_id());
      break;
    }
    case CastMessageUtilInputs::kCreateMediaRequestInput: {
      const auto& input = input_union.create_media_request_input();
      auto type = static_cast<V2MessageType>(input.type());
      if (IsMediaRequestMessageType(type)) {
        base::Value::Dict body = MakeDict(input.body());
        body.Set("type", *EnumToString(type));
        CreateMediaRequest(body, input.request_id(), input.source_id(),
                           input.destination_id());
      }
      break;
    }
    case CastMessageUtilInputs::kCreateSetVolumeRequestInput: {
      const auto& input = input_union.create_set_volume_request_input();
      base::Value::Dict body = MakeDict(input.body());
      body.Set("type",
               EnumToString<V2MessageType, V2MessageType::kSetVolume>());
      CreateSetVolumeRequest(body, input.request_id(), input.source_id());
      break;
    }
    case CastMessageUtilInputs::kIntInput: {
      IsMediaRequestMessageType(
          static_cast<V2MessageType>(input_union.int_input()));
      ToString(static_cast<GetAppAvailabilityResult>(input_union.int_input()));
      ToString(static_cast<CastMessageType>(input_union.int_input()));
      ToString(static_cast<V2MessageType>(input_union.int_input()));
      break;
    }
    case CastMessageUtilInputs::kStringInput: {
      IsCastReservedNamespace(input_union.string_input());
      break;
    }
    case CastMessageUtilInputs::kCastMessage: {
      const auto& message = input_union.cast_message();
      IsCastMessageValid(message);
      IsAuthMessage(message);
      IsReceiverMessage(message);
      IsPlatformSenderMessage(message);
      break;
    }
    case CastMessageUtilInputs::kCreateVirtualConnectionRequestInput: {
      const auto& input = input_union.create_virtual_connection_request_input();
      CreateVirtualConnectionRequest(
          input.source_id(), input.destination_id(),
          static_cast<VirtualConnectionType>(input.connection_type()),
          input.user_agent(), input.browser_version());
      break;
    }
    case CastMessageUtilInputs::kCreateGetAppAvailabilityRequestInput: {
      const auto& input =
          input_union.create_get_app_availability_request_input();
      CreateGetAppAvailabilityRequest(input.source_id(), input.request_id(),
                                      input.app_id());
      break;
    }
    case CastMessageUtilInputs::kGetRequestIdFromResponseInput: {
      const auto& input = input_union.get_request_id_from_response_input();
      base::Value::Dict payload = MakeDict(input.payload());
      if (input.has_request_id())
        payload.Set("requestId", input.request_id());
      GetRequestIdFromResponse(payload);
      break;
    }
    case CastMessageUtilInputs::kGetLaunchSessionResponseInput: {
      const auto& input = input_union.get_launch_session_response_input();
      base::Value::Dict payload = MakeDict(input.payload());
      GetLaunchSessionResponse(payload);
      break;
    }
    case CastMessageUtilInputs::kParseMessageTypeFromPayloadInput: {
      const auto& input = input_union.parse_message_type_from_payload_input();
      base::Value::Dict payload = MakeDict(input.payload());
      if (input.has_type())
        payload.Set("type", input.type());
      ParseMessageTypeFromPayload(payload);
      break;
    }
    case CastMessageUtilInputs::kCreateReceiverStatusRequestInput: {
      const auto& input = input_union.create_receiver_status_request_input();
      CreateReceiverStatusRequest(input.source_id(), input.request_id());
      break;
    }
    case CastMessageUtilInputs::INPUT_NOT_SET:
      break;
  }
}

}  // namespace fuzz
}  // namespace cast_channel
