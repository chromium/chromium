// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_gcm_server_sync_invalidation_sender.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace fake_server {

FakeGCMServerSyncInvalidationSender::FakeGCMServerSyncInvalidationSender() =
    default;

FakeGCMServerSyncInvalidationSender::~FakeGCMServerSyncInvalidationSender() =
    default;

void FakeGCMServerSyncInvalidationSender::InitializeAndListen(
    net::test_server::EmbeddedTestServer& embedded_test_server) {
  CHECK(!initialized_);
  initialized_ = true;
  fake_gcm_server_.InitializeAndListen();
  fake_gcm_server_.RegisterRequestHandler(embedded_test_server);
}

void FakeGCMServerSyncInvalidationSender::AppendCommandLineSwitches(
    base::CommandLine* cl,
    const net::test_server::EmbeddedTestServer& embedded_test_server) const {
  CHECK(initialized_);
  fake_gcm_server_.AppendCommandLineSwitches(cl, embedded_test_server);
}

void FakeGCMServerSyncInvalidationSender::Start(
    FakeServer* fake_server,
    const base::FilePath& storage_dir) {
  CHECK(initialized_);
  CHECK(!started_);
  started_ = true;
  FakeSyncInvalidationSender::Start(fake_server, storage_dir);
  fake_gcm_server_.Start(storage_dir);
}

void FakeGCMServerSyncInvalidationSender::SetOfflineAllClients(bool offline) {
  // Going back online (`!offline`) is a no-op here: `SyncTest::EnableNetwork()`
  // notifies `NetworkConnectionTracker`, which triggers
  // `gcm::ConnectionFactoryImpl` to reconnect to `FakeMCSServer` and receive
  // any queued messages upon login.
  if (offline && started_) {
    fake_gcm_server_.DisconnectAllClients();
  }
}

void FakeGCMServerSyncInvalidationSender::SendInvalidations(
    base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
        token_to_payload) {
  CHECK(started_);
  for (const auto& [token, payload] : token_to_payload) {
    fake_gcm_server_.SendMessage(token, payload.SerializeAsString());
  }
}

}  // namespace fake_server
