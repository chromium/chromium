// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_

#include <string>

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
  // across `PRE_` tests.
  virtual void Start(FakeServer* fake_server,
                     const base::FilePath& storage_dir);

  // Simulates going offline/online for all clients.
  virtual void SetOfflineAllClients(bool offline) = 0;

  // FakeServer::Observer:
  void OnWillCommit() final;
  void OnCommit(syncer::DataTypeSet committed_data_types) final;

 protected:
  // Delivers `token_to_payload` (keyed by FCM registration token) to clients.
  virtual void SendInvalidations(
      base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
          token_to_payload) = 0;

 private:
  // Updates `token_to_interested_data_types_` from `DeviceInfo` data type.
  void UpdateTokenToInterestedDataTypesMap();

  base::ScopedObservation<FakeServer, FakeServer::Observer>
      fake_server_observation_{this};

  // List of tokens with a list of interested data types. Used to send
  // invalidations to a corresponding client.
  base::flat_map<std::string, syncer::DataTypeSet>
      token_to_interested_data_types_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_H_
