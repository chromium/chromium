// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_GCM_SERVER_SYNC_INVALIDATION_SENDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_GCM_SERVER_SYNC_INVALIDATION_SENDER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/gcm_driver/fake_gcm_server.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_sync_invalidation_sender.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace fake_server {

// `FakeSyncInvalidationSender` implementation that delivers sync
// invalidations over loopback HTTP and TCP sockets via `gcm::FakeGCMServer`.
class FakeGCMServerSyncInvalidationSender : public FakeSyncInvalidationSender {
 public:
  FakeGCMServerSyncInvalidationSender();
  ~FakeGCMServerSyncInvalidationSender() override;

  // FakeSyncInvalidationSender:
  void InitializeAndListen(
      net::test_server::EmbeddedTestServer& embedded_test_server) override;
  void AppendCommandLineSwitches(base::CommandLine* cl,
                                 const net::test_server::EmbeddedTestServer&
                                     embedded_test_server) const override;
  void Start(FakeServer* fake_server,
             const base::FilePath& storage_dir) override;
  void SetOfflineAllClients(bool offline) override;

 protected:
  // FakeSyncInvalidationSender:
  void SendInvalidations(
      base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
          token_to_payload) override;

 private:
  gcm::FakeGCMServer fake_gcm_server_;
  bool initialized_ = false;
  bool started_ = false;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_GCM_SERVER_SYNC_INVALIDATION_SENDER_H_
