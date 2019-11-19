// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_EVENT_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_EVENT_STORE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_store.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace feature_engagement {

// A PersistentEventStore provides a DB layer that persists the data to disk.
// The data is retrieved once during the load process and after that this store
// is write only.  Data will be persisted asynchronously so it is not guaranteed
// to always save every write during shutdown.
class PersistentEventStore : public EventStore {
 public:
  // Builds a PersistentEventStore backed by the ProtoDatabase |db|.
  PersistentEventStore(std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db);
  ~PersistentEventStore() override;

  // EventStore implementation.
  void Load(const OnLoadedCallback& callback) override;
  bool IsReady() const override;
  void WriteEvent(const Event& event) override;
  void DeleteEvent(const std::string& event_name) override;

 private:
  void OnInitComplete(const OnLoadedCallback& callback,
                      leveldb_proto::Enums::InitStatus status);
  void OnLoadComplete(const OnLoadedCallback& callback,
                      bool success,
                      std::unique_ptr<std::vector<Event>> entries);

  const base::FilePath storage_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db_;

  // Whether or not the underlying ProtoDatabase is ready.  This will be false
  // until the OnLoadedCallback is broadcast.  It will also be false if loading
  // fails.
  bool ready_;

  base::WeakPtrFactory<PersistentEventStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PersistentEventStore);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_EVENT_STORE_H_
