// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "components/sync/protocol/proto_memory_estimations.h"

#include <string>

#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/protocol/proto_visitors.h"

namespace {

// This class is a VisitProtoFields()-compatible visitor that estimates
// proto's memory usage:
//
//  MemoryUsageVisitor visitor;
//  VisitProtoFields(visitor, proto);
//  size_t memory_usage = visitor.memory_usage();
//
class MemoryUsageVisitor {
 public:
  MemoryUsageVisitor() : memory_usage_(0) {}

  size_t memory_usage() const { return memory_usage_; }

  template <class P>
  void VisitBytes(const P& parent_proto,
                  const char* field_name,
                  const std::string& field) {
    // Delegate to Visit(..., const std::string&) below.
    Visit(parent_proto, field_name, field);
  }

  template <class P, class E>
  void VisitEnum(const P&, const char* field_name, E field) {}

  // Types derived from MessageLite (i.e. protos)
  template <class P, class F>
  typename std::enable_if<
      std::is_base_of<google::protobuf::MessageLite, F>::value>::type
  Visit(const P&, const char* field_name, const F& field) {
    using base::trace_event::EstimateMemoryUsage;
    // All object fields are dynamically allocated.
    memory_usage_ += sizeof(F) + EstimateMemoryUsage(field);
  }

  // Arithmetic types
  template <class P, class F>
  typename std::enable_if<std::is_arithmetic<F>::value>::type
  Visit(const P&, const char* field_name, const F& field) {
    // Arithmetic fields (integers, floats & bool) don't allocate.
  }

  // std::string
  template <class P>
  void Visit(const P&, const char* field_name, const std::string& field) {
    using base::trace_event::EstimateMemoryUsage;
    // All strings are of type ArenaStringPtr, which essentially
    // is std::string*.
    memory_usage_ += sizeof(std::string) + EstimateMemoryUsage(field);
  }

  // RepeatedPtrField
  template <class P, class F>
  void Visit(const P&,
             const char* field_name,
             const google::protobuf::RepeatedPtrField<F>& fields) {
    using base::trace_event::EstimateMemoryUsage;
    // Can't use RepeatedPtrField::SpaceUsedExcludingSelf() because it will
    // end up calling undefined TypeHandler::SpaceUsed() method.
    memory_usage_ += fields.Capacity() ? sizeof(void*) : 0;  // header
    memory_usage_ += fields.Capacity() * sizeof(void*);
    for (const auto& field : fields) {
      memory_usage_ += sizeof(F) + EstimateMemoryUsage(field);
    }
  }

  // RepeatedField<arithmetic type>
  template <class P, class F>
  typename std::enable_if<std::is_arithmetic<F>::value>::type Visit(
      const P&,
      const char* field_name,
      const google::protobuf::RepeatedField<F>& fields) {
    memory_usage_ += fields.SpaceUsedExcludingSelf();
    // Arithmetic fields (integers, floats & bool) don't allocate, so no point
    // in iterating over |fields|.
  }

  // RepeatedField<std::string>
  template <class P>
  void Visit(const P&,
             const char* field_name,
             const google::protobuf::RepeatedField<std::string>& fields) {
    using base::trace_event::EstimateMemoryUsage;
    memory_usage_ += fields.SpaceUsedExcludingSelf();
    for (const auto& field : fields) {
      memory_usage_ += EstimateMemoryUsage(field);
    }
  }

 private:
  size_t memory_usage_;
};

}  // namespace

namespace sync_pb {

template <class P>
size_t EstimateMemoryUsage(const P& proto) {
  MemoryUsageVisitor visitor;
  syncer::VisitProtoFields(visitor, proto);
  return visitor.memory_usage();
}

// Explicit instantiations

#define INSTANTIATE(Proto) \
  template size_t EstimateMemoryUsage<Proto>(const Proto&);

INSTANTIATE(DataTypeContext)
INSTANTIATE(DataTypeProgressMarker)
INSTANTIATE(EntityMetadata)
INSTANTIATE(EntitySpecifics)
INSTANTIATE(ModelTypeState)
INSTANTIATE(PersistedEntityData)
INSTANTIATE(UniquePosition)

}  // namespace sync_pb
