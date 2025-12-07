// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_

#include <cstdio>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_status.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// Returns the value of `proto_field` from `msg`.
// Returns nullopt when the `proto_field` does not reference a valid field, or
// has a type that cannot be coerced to proto::Value.
std::optional<proto::Value> GetProtoValue(
    const google::protobuf::MessageLite& msg,
    const proto::ProtoField& proto_field);

// Casts the serialized proto in `msg` to the type in `msg.type_url`.
// Returns a unique_ptr to the casted proto field.
std::unique_ptr<google::protobuf::MessageLite> GetProtoFromAny(
    const proto::Any& msg);

// Builds a new proto message of type `proto_name`.
std::unique_ptr<google::protobuf::MessageLite> BuildMessage(
    const std::string& proto_name);

// Sets `value` in `msg`'s `proto_field`. Converts to non-string fields using
// ValueConverter<T>::TryConvertFromString(). Returns kError if the value is not
// convertible to the `proto_field` type.
ProtoStatus SetProtoValueFromString(google::protobuf::MessageLite* msg,
                                    const proto::ProtoField& proto_field,
                                    const std::string& value);

// Get immutable value for a singular message field.
// Analogous to google::protobuf::Reflection::GetMessage.
const google::protobuf::MessageLite* GetProtoMessage(
    const google::protobuf::MessageLite* msg,
    int32_t tag_number);

// Get mutable value for a singular message field.
// Analogous to google::protobuf::Reflection::MutableMessage.
google::protobuf::MessageLite* GetProtoMutableMessage(
    google::protobuf::MessageLite* msg,
    int32_t tag_number);

// Appends a new empty message to a repeated message field.
// Returns the number of elements in the field after adding it (or zero if
// the field or message is not supported)
int AddProtoMessage(google::protobuf::MessageLite* msg, int32_t tag_number);

// Gets a mutable message for one value from a repeated message field.
// 'tag_number' identifies the field, and 'offset' is which value.
// Analogous to google::protobuf::Reflection::MutableRepeatedMessage.
google::protobuf::MessageLite* GetProtoMutableRepeatedMessage(
    google::protobuf::MessageLite* parent,
    int32_t tag_number,
    int offset);

// Returns the size of a repeated message field.
int GetProtoRepeatedSize(const google::protobuf::MessageLite* msg,
                         int32_t tag_number);

// Set the field of 'msg' with the given 'tag' to have provided 'value'.
ProtoStatus SetProtoField(google::protobuf::MessageLite* msg,
                          int32_t tag,
                          const std::string& value);

// Converts a base::Value to a proto of the given type, wrapped in a proto::Any.
std::optional<proto::Any> ConvertToAnyWrappedProto(
    const base::Value& object,
    const std::string& type_name);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_
