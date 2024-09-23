// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_ARENA_PERSISTENT_PROTO_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_ARENA_PERSISTENT_PROTO_H_

#include <concepts>
#include <memory>
#include <type_traits>

#include "components/metrics/structured/lib/persistent_proto_internal.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {

// A PersistentProto that stores T in an arena.
//
// The provided arena must live longer than |this| and doesn't
// take ownership.
//
// See comment in persistent_proto.h and persistent_proto_internal.h for more
// details.
template <class T>
  requires(std::derived_from<T, google::protobuf::MessageLite>)
class ArenaPersistentProto : public internal::PersistentProtoInternal {
 public:
  ArenaPersistentProto(const base::FilePath& path,
                       base::TimeDelta write_delay,
                       PersistentProtoInternal::ReadCallback on_read,
                       PersistentProtoInternal::WriteCallback on_write)
      : internal::PersistentProtoInternal(path,
                                          write_delay,
                                          std::move(on_read),
                                          std::move(on_write)) {}

  ~ArenaPersistentProto() override { DeallocProto(); }

  T* get() { return static_cast<T*>(internal::PersistentProtoInternal::get()); }
  const T* get() const {
    return static_cast<T*>(internal::PersistentProtoInternal::get());
  }

  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

  T& operator*() { return *get(); }
  const T& operator*() const { return *get(); }

  const google::protobuf::Arena* arena() const { return &arena_; }

 private:
  google::protobuf::MessageLite* GetProto() override {
    return google::protobuf::Arena::Create<T>(&arena_);
  }

  google::protobuf::Arena arena_;
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_ARENA_PERSISTENT_PROTO_H_
