// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_FLUSHED_MAP_H_
#define COMPONENTS_METRICS_STRUCTURED_FLUSHED_MAP_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/metrics/structured/lib/event_buffer.h"  // EventBuffer and FlushError
#include "components/metrics/structured/lib/resource_info.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace metrics::structured {

// Manages the reading and writing of events written to disk. All write
// operations are enqueued to the same task runner.
//
// Writing a flushed file is performed asynchronously while reading is performed
// synchronously. This is because the Flush API is designed to be called from
// the UI thread where blocking operations are not allowed. The Read API
// is intended to be called from an IO task while final logs are prepared.
//
// When an EventBuffer is flushed, a FlushedKey is used to represent the on-disk
// data and provide some metadata about what is stored. This key is then used to
// read or delete the events.
//
// All events that have been flushed to disk are considered ready to be
// uploaded.
class FlushedMap {
 public:
  FlushedMap(const base::FilePath& flushed_dir, uint64_t max_size_bytes);

  ~FlushedMap();

  // Deletes all flushed events from disk.
  void Purge();

  // Flushes |buffer| to disk. A key is returned that is used to identify the
  // on-disk events.
  //
  // |buffer| defines how the flushing occurs.
  void Flush(EventBuffer<StructuredEventProto>& buffer,
             FlushedCallback callback);

  // Reads the events stored at |key|.
  std::optional<EventsProto> ReadKey(const FlushedKey& key) const;

  // Deletes the events of |key|.
  void DeleteKey(const FlushedKey& key);

  void DeleteKeys(const std::vector<FlushedKey>& keys);

  const std::vector<FlushedKey>& keys() const { return keys_; }

  const ResourceInfo& resource_info() const { return resource_info_; }

  bool empty() const { return keys().empty(); }

 private:
  // Generates a new path under |flushed_dir_| to store the events. The
  // filename is generated using UUID.
  base::FilePath GenerateFilePath() const;

  // Starts a task that builds the list of in-memory keys.
  void LoadKeysFromDir(const base::FilePath& dir);

  // Traverses |dir| building a list of keys.
  //
  // It is assumed that all of the files in |dir| store serialized EventsProtos.
  void BuildKeysFromDir(const base::FilePath& dir);

  // Flushed map operations that need to be handled post flush.
  void OnFlushed(FlushedCallback callback,
                 base::expected<FlushedKey, FlushError> key);

  // The directory where events are flushed.
  base::FilePath flushed_dir_;

  // List of all the keys for flushed events.
  std::vector<FlushedKey> keys_;

  // Manages the amount of resource used by |this|.
  ResourceInfo resource_info_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<FlushedMap> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_FLUSHED_MAP_H_
