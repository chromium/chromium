// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_

#include <concepts>
#include <type_traits>

#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {

// The current usage and limits of some recourse.
//
// These resources could be disk space or memory consumption.
struct ResourceInfo {
  int32_t used_size_bytes;
  int32_t max_size_bytes;

  explicit ResourceInfo(int32_t max_size_bytes);
  ResourceInfo(int32_t used_size_bytes, int32_t max_size_bytes);

  // Check whether |this| can accommodate |size|.
  bool HasRoom(int32_t size) const;

  // Increases currently used space with |size|.
  bool Consume(int32_t size);
};

enum class Result {
  // Event was added successfully.
  kOk = 0,
  // Event was added but another might not fit.
  kShouldFlush = 1,
  // Event was not added. Must flush now.
  kFull = 2,
  // Any error that occurred.
  kError = 3,
};

// Abstraction for how in-memory events are managed on device.
template <typename T>
  requires(std::derived_from<T, google::protobuf::MessageLite>)
class EventBuffer {
 public:
  explicit EventBuffer(ResourceInfo info) : resource_info_(info) {}

  virtual ~EventBuffer() = default;

  // Adds an event to |this| buffer.
  virtual Result AddEvent(T event) = 0;

  // Clears the content of the buffer.
  virtual void Purge() = 0;

  // The number of events stored in the buffer.
  virtual uint64_t Size() = 0;

  const ResourceInfo& resource_info() const { return resource_info_; }

 protected:
  ResourceInfo resource_info_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_
