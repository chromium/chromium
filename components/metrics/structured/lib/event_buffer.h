// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_

#include <concepts>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/metrics/structured/lib/resource_info.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {
// The result of adding an event to a buffer.
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

// Errors of flushing a buffer to disk.
enum FlushError {
  kQuotaExceeded = 1,
  kWriteError = 2,
  kDiskFull = 3,
  kSerializationFailed = 4,
};

// Represents a flushed buffer.
struct FlushedKey {
  // The size of the file flushed.
  int64_t size;
  // Path of the file.
  base::FilePath path;
  // When the flushed file was created.
  base::Time creation_time;
};

using FlushedCallback =
    base::OnceCallback<void(base::expected<FlushedKey, FlushError>)>;

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

  // Serialize the contents of |this|.
  virtual google::protobuf::RepeatedPtrField<T> Serialize() = 0;

  // Flushes the buffer to |path|, once the flush is complete |callback| is
  // executed.
  virtual void Flush(const base::FilePath& path, FlushedCallback callback) = 0;

  const ResourceInfo& resource_info() const { return resource_info_; }

 protected:
  ResourceInfo resource_info_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_BUFFER_H_
