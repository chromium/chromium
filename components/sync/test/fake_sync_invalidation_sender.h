// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_server.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace fake_server {

// Base class observing changes to the fake server and building invalidation
// payloads upon commits. Sent invalidation follows the same format expected by
// the sync invalidations framework (i.e. `SyncInvalidationsService`).
class FakeSyncInvalidationSender : public FakeServer::Observer {
 public:
  FakeSyncInvalidationSender();
  ~FakeSyncInvalidationSender() override;
  FakeSyncInvalidationSender(const FakeSyncInvalidationSender&) = delete;
  FakeSyncInvalidationSender& operator=(const FakeSyncInvalidationSender&) =
      delete;

  // Initializes the underlying fake GCM server (if any) and registers HTTP
  // handlers on `embedded_test_server`. Must be called before
  // `AppendCommandLineSwitches()`.
  virtual void InitializeAndListen(
      net::test_server::EmbeddedTestServer& embedded_test_server);

  // Appends GCM/MCS endpoint switches to `cl` (if any).
  // `embedded_test_server` must already be initialized and listening.
  virtual void AppendCommandLineSwitches(
      base::CommandLine* cl,
      const net::test_server::EmbeddedTestServer& embedded_test_server) const;

  // Starts observing `fake_server` (which must not be null and must outlive
  // this object) and initializes storage in `storage_dir` to persist state
  // across `PRE_` tests. When `reflection_blocking_enabled` is true,
  // invalidations caused by a client's own commits (matching
  // `committer_cache_guid`) are suppressed, matching production server
  // reflection blocking.
  virtual void Start(FakeServer* fake_server,
                     const base::FilePath& storage_dir,
                     bool reflection_blocking_enabled);

  // Simulates going offline/online for all clients.
  virtual void SetOfflineAllClients(bool offline) = 0;

  // FakeServer::Observer:
  void OnWillCommit() final;
  void OnCommit(std::string_view committer_cache_guid,
                syncer::DataTypeSet committed_data_types) final;

 protected:
  // Delivers `token_to_payload` (keyed by FCM registration token) to clients.
  virtual void SendInvalidations(
      base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
          token_to_payload) = 0;

 private:
  struct SubscribedClientInfo {
    std::string cache_guid;
    syncer::DataTypeSet interested_data_types;
  };

  // Updates `token_to_subscribed_client_` from `DeviceInfo` data type.
  void UpdateSubscribedClientsMap();

  bool reflection_blocking_enabled_ = false;

  base::ScopedObservation<FakeServer, FakeServer::Observer>
      fake_server_observation_{this};

  // Map from FCM registration token to its subscribed client info.
  base::flat_map<std::string, SubscribedClientInfo> token_to_subscribed_client_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_
