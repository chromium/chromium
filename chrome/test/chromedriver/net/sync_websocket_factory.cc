// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/log_replay/log_replay_socket.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

namespace {

std::unique_ptr<SyncWebSocket> CreateSyncWebSocket(
    scoped_refptr<URLRequestContextGetter> context_getter) {
  return std::unique_ptr<SyncWebSocket>(
      new SyncWebSocketImpl(context_getter.get()));
}

std::unique_ptr<SyncWebSocket> CreateReplayWebSocket(base::FilePath log_path) {
  return std::make_unique<LogReplaySocket>(log_path);
}

}  // namespace

SyncWebSocketFactory CreateSyncWebSocketFactory(
    URLRequestContextGetter* getter) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch("devtools-replay")) {
    base::CommandLine::StringType log_path_str =
        cmd_line->GetSwitchValueNative("devtools-replay");
    base::FilePath log_path(log_path_str);
    return base::BindRepeating(&CreateReplayWebSocket, log_path);
  }
  return base::BindRepeating(&CreateSyncWebSocket,
                             base::WrapRefCounted(getter));
}
