// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_SERVER_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_SERVER_H_

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "google_apis/gcm/engine/fake_mcs_server.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace gcm {

// A fake GCM/FCM server for testing that implements:
// 1. HTTP check-in and registration endpoints, hookable into
//    `net::test_server::EmbeddedTestServer` via `RegisterRequestHandler()`.
// 2. Binary MCS streaming message delivery via `FakeMCSServer`.
// 3. Disk persistence (`fake_gcm_server_state.json`) across `PRE_` restarts.
class FakeGCMServer {
 public:
  static constexpr char kCheckinPath[] = "/gcm/checkin";
  static constexpr char kRegisterPath[] = "/gcm/register";

  FakeGCMServer();
  FakeGCMServer(const FakeGCMServer&) = delete;
  FakeGCMServer& operator=(const FakeGCMServer&) = delete;
  ~FakeGCMServer();

  // Binds the local TCP listening socket for the underlying `FakeMCSServer` so
  // `GetMCSEndpointURL()` can be queried before `Start()` is called.
  void InitializeAndListen();

  // Loads persisted device credentials, token registrations, and unacknowledged
  // MCS messages from `storage_dir` and starts the background MCS IO thread.
  // `InitializeAndListen()` must be called before `Start()`.
  void Start(const base::FilePath& storage_dir);

  // Registers HTTP request handlers for check-in and registration on
  // `embedded_test_server`.
  void RegisterRequestHandler(
      net::test_server::EmbeddedTestServer& embedded_test_server);

  // Appends the command-line switches (`--gcm-checkin-url`,
  // `--gcm-registration-url`, and `--gcm-mcs-endpoint`) pointing
  // `GCMClientImpl` at `embedded_test_server` and `GetMCSEndpointURL()`.
  // `InitializeAndListen()` and `embedded_test_server.InitializeAndListen()`
  // must be called before `AppendCommandLineSwitches()`.
  void AppendCommandLineSwitches(
      base::CommandLine* command_line,
      const net::test_server::EmbeddedTestServer& embedded_test_server) const;

  // Returns the local URL (`http://127.0.0.1:<port>`) where the binary MCS TCP
  // server is listening. `InitializeAndListen()` must be called first.
  GURL GetMCSEndpointURL() const;

  // Sends a push message with `raw_data` to the device/application registered
  // with `registration_token`. If the token is registered, queues an MCS
  // `DataMessageStanza` for its device via `FakeMCSServer` and returns true.
  // Returns false if `registration_token` is not registered.
  bool SendMessage(std::string_view registration_token,
                   std::string_view raw_data);

  // Closes all active client MCS TCP connections.
  void DisconnectAllClients();

 private:
  class StateCore;

  scoped_refptr<StateCore> state_core_;
  FakeMCSServer mcs_server_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_SERVER_H_
