// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STORAGE_MANAGER_H_
#define COMPONENTS_METRICS_STRUCTURED_STORAGE_MANAGER_H_

#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/lib/event_storage.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

// Enum representing why events were deleted.
enum class DeleteReason {
  // The events have been uploaded. The events were not lost.
  kUploaded,
  // The events were deleted because we are consuming to much disk space. Events
  // are lost.
  kExceededQuota,
};

// The Storage Manager is responsible for storing and managing Structured
// Metrics events.
//
// The Storage Manager is responsible for the following:
//   * Flushing events to a local file when memory is exceeded.
//   * Dropping events when storage is exceeded.
//   * Retrieving events for upload.
//
// The Storage Manager is not thread safe, with the exception of the
// StorageService interface. Therefore, construction and destruction must be
// done on the same sequence. This class exists to provide an API to
// StructuredMetricsService which only exists in //components.
class StorageManager : public EventStorage<StructuredEventProto> {
 public:
  // An object that is using the Storage Manager for storing events.
  //
  // This interface is used to by the Storage Manager to provide information
  // externally.
  class StorageDelegate {
   public:
    StorageDelegate() = default;
    virtual ~StorageDelegate() = default;

    // Called when a buffer has been flushed to disk.
    virtual void OnFlushed(const FlushedKey& key) = 0;

    // Called when flushed events have been deleted.
    virtual void OnDeleted(const FlushedKey& key, DeleteReason reason) = 0;
  };

  // Ideally, this would take a StorageDelegate as a parameter but because of
  // how the StructuredMetricsService needs to be constructed, this isn't
  // possible right now.
  StorageManager();

  ~StorageManager() override;

  // Should only be called by the owner of |this|.
  void set_delegate(StorageDelegate* delegate) {
    CHECK(delegate);
    delegate_ = delegate;
  }

  void unset_delegate(StorageDelegate* delegate) {
    if (delegate_ == delegate) {
      delegate_ = nullptr;
    }
  }

 protected:
  void NotifyOnFlushed(const FlushedKey& key);

  void NotifyOnDeleted(const FlushedKey& key, DeleteReason reason);

  // The owner of |this|.
  raw_ptr<StorageDelegate> delegate_ = nullptr;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STORAGE_MANAGER_H_
