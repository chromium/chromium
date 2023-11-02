// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_
#define COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_

#include <stdint.h>

#include <memory>

#include "base/process/process.h"
#include "base/run_loop.h"
#include "components/nacl/common/nacl_types.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
}

// The BrokerThread class represents the thread that handles the messages from
// the browser process and starts NaCl loader processes.
class NaClBrokerListener : public content::SandboxedProcessLauncherDelegate,
                           public IPC::Listener {
 public:
  NaClBrokerListener();

  NaClBrokerListener(const NaClBrokerListener&) = delete;
  NaClBrokerListener& operator=(const NaClBrokerListener&) = delete;

  ~NaClBrokerListener() override;

  void Listen();

  // content::SandboxedProcessLauncherDelegate implementation:
  sandbox::mojom::Sandbox GetSandboxType() override;
  std::string GetSandboxTag() override;

  // IPC::Listener implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

 private:
  void OnLaunchLoaderThroughBroker(
      int launch_id,
      mojo::MessagePipeHandle service_request_pipe);
  void OnLaunchDebugExceptionHandler(int32_t pid,
                                     base::ProcessHandle process_handle,
                                     const std::string& startup_info);
  void OnStopBroker();

  base::RunLoop run_loop_;
  base::Process browser_process_;
  std::unique_ptr<IPC::Channel> channel_;
};

#endif  // COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_
