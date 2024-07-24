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
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// A utility object for iterating over nested messages as MessageLite objects.
// This is basically a workaround for the fact that there is no public common
// base class for RepeatedPtrField<Msg>.
class NestedMessageIterator {
 public:
  NestedMessageIterator(const google::protobuf::MessageLite* parent,
                        int32_t tag_number,
                        int32_t field_size,
                        int32_t offset);

  NestedMessageIterator begin() const {
    return NestedMessageIterator(parent_, tag_number_, field_size_, 0);
  }

  NestedMessageIterator end() const {
    return NestedMessageIterator(parent_, tag_number_, field_size_,
                                 field_size_);
  }

  bool operator==(const NestedMessageIterator& rhs) const {
    DCHECK_EQ(tag_number_, rhs.tag_number_);
    DCHECK_EQ(field_size_, rhs.field_size_);
    return offset_ == rhs.offset_;
  }

  bool operator!=(const NestedMessageIterator& rhs) const {
    return !operator==(rhs);
  }

  const google::protobuf::MessageLite* operator*() const { return Get(); }

  NestedMessageIterator& operator++() {
    Advance();
    return *this;
  }

 private:
  void Advance() { ++offset_; }

  const google::protobuf::MessageLite* Get() const;

  const raw_ptr<const google::protobuf::MessageLite> parent_;
  int32_t tag_number_;
  int32_t field_size_;
  int32_t offset_ = 0;
};

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

// Constructs a new proto of `proto_name` type, and sets `value` in it's
// `proto_field` and returns it wrapped in a proto::Any.
// Returns nullopt if `proto_field` is not a valid string type field.
std::optional<proto::Any> SetProtoValue(const std::string& proto_name,
                                        const proto::ProtoField& proto_field,
                                        const std::string& value);

// Returns all of the values of some repeated field.
//
// Returns nullopt if proto_field does not reference a valid repeated field.
// The return result should be used via a range-based for loop.
std::optional<NestedMessageIterator> GetProtoRepeated(
    const google::protobuf::MessageLite* msg,
    const proto::ProtoField& proto_field);

// Converts a base::Value to a proto of the given type, wrapped in a proto::Any.
std::optional<proto::Any> ConvertToAnyWrappedProto(
    const base::Value& object,
    const std::string& type_name);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_
