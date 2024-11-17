// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_H_

#include <concepts>
#include <memory>
#include <type_traits>

#include "components/metrics/structured/lib/persistent_proto_internal.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {

// PersistentProto wraps a proto class and persists it to disk. Usage summary.
//  - Init is asynchronous, usage before |on_read| is called will crash.
//  - pproto->Method() will call Method on the underlying proto.
//  - Call QueueWrite() to write to disk.
//
// Reading. The backing file is read asynchronously from disk once at
// initialization, and the |on_read| callback is run once this is done. Until
// |on_read| is called, has_value is false and get() will always return nullptr.
// If no proto file exists on disk, or it is invalid, a blank proto is
// constructed and immediately written to disk.
//
// Writing. Writes must be triggered manually. QueueWrite() delays writing to
// disk for |write_delay| time, in order to batch successive writes.
// The |on_write| callback is run each time a write has completed. QueueWrite()
// should not be called until OnReadComplete() is finished, which can be
// checked with the callback |on_read_|. Calling QueueWrite() before
// OnReadComplete() has finished will result in a crash.
//
// The |on_write| callback is run each time a write has completed.
//
// This class is NOT thread-safe and access to the proto should be on the same
// sequence |this| is constructed.
template <class T>
  requires(std::derived_from<T, google::protobuf::MessageLite>)
class PersistentProto : public internal::PersistentProtoInternal {
 public:
  using internal::PersistentProtoInternal::PersistentProtoInternal;

  PersistentProto(const base::FilePath& path,
                  base::TimeDelta write_delay,
                  PersistentProtoInternal::ReadCallback on_read,
                  PersistentProtoInternal::WriteCallback on_write)
      : internal::PersistentProtoInternal(path,
                                          write_delay,
                                          std::move(on_read),
                                          std::move(on_write)),
        handle_(std::make_unique<T>()) {}

  ~PersistentProto() override { DeallocProto(); }

  T* get() { return static_cast<T*>(internal::PersistentProtoInternal::get()); }
  const T* get() const {
    return static_cast<T*>(internal::PersistentProtoInternal::get());
  }

  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

  T& operator*() { return *get(); }
  const T& operator*() const { return *get(); }

 private:
  google::protobuf::MessageLite* GetProto() override { return handle_.get(); }

  std::unique_ptr<T> handle_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_H_
